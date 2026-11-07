// trees.h —— 拥有型森林（一批树）容器，基于 intrusive_list.h 实现。
//
// 表示：
//   森林 = 一条根链；每个节点自带一条孩子链（链头是该节点的成员）。
//   节点只有一个 sibling 钩子（intrusive_list_node_base 的 prev/next）：
//     - 根节点挂在森林的根链上；
//     - 非根节点挂在父节点的孩子链上。
//   于是：
//     * 切下一棵子树 = 一次 release()，O(1)，与子树大小无关；
//     * 整片森林 / 整条孩子链的挂接 = 一次 splice()，O(1)；
//     * 节点是侵入式的，分配后地址永不改变，node_proxy 直接包住链表迭代器。
//
// 遍历入口：
//   begin_tree()/end_tree()         走根链（森林里的每一棵树）；
//   node_proxy::child_begin()/end() 下钻到某棵树的孩子链。
//
// 本版本没有父指针：只能向下走，不能从节点回到父节点。
// 节点由容器拥有：分配、构造、析构、回收都由 trees 负责。
#pragma once

// MSVC 不支持标准 [[no_unique_address]]，要用它的 [[msvc::no_unique_address]]
#ifndef YC_NO_UNIQUE_ADDRESS
#if defined(_MSC_VER)
#define YC_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#else
#define YC_NO_UNIQUE_ADDRESS [[no_unique_address]]
#endif
#endif

#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <ranges>
#include <type_traits>
#include <utility>

#include "intrusive_list.h"

namespace Yc
{
    namespace details
    {
        // 森林节点 = 侵入式链表节点（sibling 钩子 + T）+ 孩子链头。
        // intrusive_list_node 已经处理了 union 里 T 的延迟构造/析构，
        // 这里只加一条孩子链；孩子链的哨兵同样内嵌在节点里。
        template<class vptr, class T>
        struct trees_node : intrusive_list_node<vptr, T>
        {
            basic_intrusive_list<vptr> children{};
        };

        // ===================================================================
        // children_view：一条兄弟链的“借用视图”，元素是节点代理。
        //
        // 视图只借 [first, last)（森林的根链，或某个节点的孩子链），不拥有节点。
        // begin()/end() 的 operator* 按值返回 Proxy（node_proxy / node_const_proxy），
        // 所以拿到的是子树根的代理，正好可以喂给 recur_and_write 的 cg：
        //   cg = [](node_proxy p) { return p.all_children(); };
        //
        // 实现在 details，通过 trees::children_view / children_const_view 公开。
        // 可以用 (first, last) 两个代理构造，也可以用 std::pair 构造。
        // ===================================================================
        template<class Proxy>
        class children_iterator
        {
            Proxy it_{};
        public:
            using value_type = Proxy;
            using reference = Proxy;                  // 解引用返回右值
            using difference_type = std::ptrdiff_t;
            using iterator_concept = std::bidirectional_iterator_tag;
            using iterator_category = std::bidirectional_iterator_tag;

            children_iterator() = default;
            explicit children_iterator(Proxy it) noexcept : it_(it) {}

            reference operator*() const noexcept
            {
                return it_;
            }
            children_iterator& operator++() noexcept
            {
                ++it_;
                return *this;
            }
            children_iterator& operator--() noexcept
            {
                --it_;
                return *this;
            }
            children_iterator operator++(int) noexcept
            {
                auto tmp = *this;
                ++it_;
                return tmp;
            }
            children_iterator operator--(int) noexcept
            {
                auto tmp = *this;
                --it_;
                return tmp;
            }
            friend bool operator==(const children_iterator&, const children_iterator&) noexcept = default;
        };

        template<class Proxy>
        class children_view : public std::ranges::view_interface<children_view<Proxy>>
        {
            Proxy first_{};
            Proxy last_{};
        public:
            children_view() = default;
            children_view(Proxy first, Proxy last) noexcept
                : first_(first), last_(last)
            {}
            // 代理对构造函数
            explicit children_view(std::pair<Proxy, Proxy> ends) noexcept
                : first_(ends.first), last_(ends.second)
            {}
            children_iterator<Proxy> begin() const noexcept
            {
                return children_iterator<Proxy>{ first_ };
            }
            children_iterator<Proxy> end() const noexcept
            {
                return children_iterator<Proxy>{ last_ };
            }
        };

        // ===================================================================
        // node_proxy：完全就是 basic_intrusive_list 迭代器的包装。
        //
        //   * operator*  返回节点携带的 T（不是节点本身）；
        //   * ++ / -- / == 直接转发给链表迭代器，因此遍历的是"兄弟"；
        //   * child_begin() / child_end() 下钻到当前节点的孩子链。
        //
        // Const = false 时包 iterator，= true 时包 const_iterator；
        // 两种特化只差这一个类型，其余逻辑完全相同。
        // ===================================================================
        template<class vptr, class T, bool Const>
        class trees_node_proxy
        {
            using list_type = basic_intrusive_list<vptr>;
            using iterator_type = std::conditional_t<Const,
                typename list_type::const_iterator,
                typename list_type::iterator>;
            using node_type = trees_node<vptr, T>;
            using node_ref = std::conditional_t<Const, const node_type&, node_type&>;

            iterator_type it{};
        public:
            using value_type = T;
            using reference = std::conditional_t<Const, const T&, T&>;
            using pointer = std::conditional_t<Const, const T*, T*>;
            using difference_type = std::ptrdiff_t;
            using iterator_category = std::bidirectional_iterator_tag;
            using iterator_concept = std::bidirectional_iterator_tag;

            trees_node_proxy() = default;
            explicit trees_node_proxy(iterator_type i) noexcept : it(i) {}

            // 允许 node_proxy -> node_const_proxy
            template<bool C = Const>
                requires (!C)
            operator trees_node_proxy<vptr, T, true>() const noexcept
            {
                return trees_node_proxy<vptr, T, true>{ it };
            }

            // 解引用拿到节点带着的 T
            reference operator*() const noexcept
            {
                return static_cast<node_ref>(*it).value();
            }
            pointer operator->() const noexcept
            {
                return std::addressof(operator*());
            }

            trees_node_proxy& operator++() noexcept
            {
                ++it;
                return *this;
            }
            trees_node_proxy& operator--() noexcept
            {
                --it;
                return *this;
            }
            trees_node_proxy operator++(int) noexcept
            {
                auto tmp = *this;
                ++*this;
                return tmp;
            }
            trees_node_proxy operator--(int) noexcept
            {
                auto tmp = *this;
                --*this;
                return tmp;
            }

            friend bool operator==(const trees_node_proxy&, const trees_node_proxy&) noexcept = default;

            // 下钻：当前节点的孩子链（叶子节点上 child_begin() == child_end()）
            trees_node_proxy child_begin() const noexcept
            {
                return trees_node_proxy{ static_cast<node_ref>(*it).children.begin() };
            }
            trees_node_proxy child_end() const noexcept
            {
                return trees_node_proxy{ static_cast<node_ref>(*it).children.end() };
            }

            // 直接孩子（每个孩子是一棵子树的根）；解引用得到 node_proxy
            children_view<trees_node_proxy> all_children() const noexcept
            {
                return children_view<trees_node_proxy>{ child_begin(), child_end() };
            }

            // 对孩子链按值稳定排序；比较器接收 const T&。
            // 只有可变代理可用，const 代理不能改结构。
            template<class Compare>
                requires (!Const)
            void sort_children(Compare comp) const
            {
                node_type& n = static_cast<node_type&>(*it);
                n.children.sort([&comp](const list_type::node_type& a, const list_type::node_type& b)
                {
                    return std::invoke(comp,
                        static_cast<const node_type&>(a).value(),
                        static_cast<const node_type&>(b).value());
                });
            }

            // 底层链表迭代器（trees 内部用来构造/比较代理）
            iterator_type base() const noexcept
            {
                return it;
            }
        };
    }

    // ===================================================================
    // trees：拥有型森林
    //
    //   根链      —— roots，森林里的每一棵树（根节点）按插入顺序串在这里；
    //   孩子链    —— 每个节点自己的 children，空链 = 哨兵自指；
    //   节点所有权 —— make_node / destroy_node 用重新绑定的分配器管理。
    // ===================================================================
    template<class T, class Alloc = std::allocator<T>>
    class trees
    {
        using vptr = typename std::allocator_traits<Alloc>::void_pointer;
        using node_type = details::trees_node<vptr, T>;
        using list_type = basic_intrusive_list<vptr>;
        using node_allocator = typename std::allocator_traits<Alloc>::template rebind_alloc<node_type>;
        using node_pointer = typename std::allocator_traits<node_allocator>::pointer;

        list_type roots;                        // 根链：森林里所有树串在这里
        YC_NO_UNIQUE_ADDRESS Alloc alloc{};

        // RAII 守卫：析构时执行 f，dismiss() 后不再执行。
        // 用来替代 try/catch 做回滚：正常路径 dismiss，异常路径析构回滚。
        template<class F>
        class scope_guard
        {
            F f_;
            bool active_ = true;
        public:
            explicit scope_guard(F f) noexcept(std::is_nothrow_move_constructible_v<F>)
                : f_(std::move(f))
            {}
            scope_guard(const scope_guard&) = delete;
            scope_guard& operator=(const scope_guard&) = delete;
            scope_guard(scope_guard&& other) noexcept(std::is_nothrow_move_constructible_v<F>)
                : f_(std::move(other.f_)), active_(std::exchange(other.active_, false))
            {}
            scope_guard& operator=(scope_guard&&) = delete;
            ~scope_guard() noexcept
            {
                if (active_)
                {
                    f_();
                }
            }
            void dismiss() noexcept
            {
                active_ = false;
            }
        };
        template<class F>
        static scope_guard<F> make_scope_guard(F f)
        {
            return scope_guard<F>{ std::move(f) };
        }

        // 分配一个节点并在它自己的存储上构造节点本体 + T。
        // 节点本体走 intrusive_list_node 的显式默认构造，让基类的 NSDMI
        // 把 prev/next 置成自指、让 children 的哨兵自指；T 随后用节点钩子构造。
        template<class... Args>
        node_type* make_node(Args&&... args)
        {
            node_allocator na{ alloc };
            node_pointer p = std::allocator_traits<node_allocator>::allocate(na, 1);
            node_type* raw = std::to_address(p);
            bool constructed = false;
            auto guard = make_scope_guard([&]() {
                if (constructed)
                {
                    std::allocator_traits<node_allocator>::destroy(na, raw);
                }
                std::allocator_traits<node_allocator>::deallocate(na, p, 1);
            });
            std::allocator_traits<node_allocator>::construct(na, raw);
            constructed = true;
            raw->construct_value(na, std::forward<Args>(args)...);
            guard.dismiss();
            return raw;
        }

        // 销毁节点：先销毁 T，再销毁节点本体，最后还内存。
        // 调用方必须保证该节点的孩子链已经空了（erase 负责这件事）。
        void destroy_node(node_type& n) noexcept
        {
            node_allocator na{ alloc };
            n.destroy_value(na);
            std::allocator_traits<node_allocator>::destroy(na, std::addressof(n));
            std::allocator_traits<node_allocator>::deallocate(
                na, std::pointer_traits<node_pointer>::pointer_to(n), 1);
        }

    public:
        using value_type = T;
        using allocator_type = Alloc;
        using pointer = typename std::allocator_traits<Alloc>::pointer;
        using const_pointer = typename std::allocator_traits<Alloc>::const_pointer;
        using void_pointer = typename std::allocator_traits<Alloc>::void_pointer;
        using const_void_pointer = typename std::allocator_traits<Alloc>::const_void_pointer;
        using reference = T&;
        using const_reference = const T&;
        using node_proxy = details::trees_node_proxy<vptr, T, false>;
        using node_const_proxy = details::trees_node_proxy<vptr, T, true>;
        // 公开的借用视图类型：元素是子树根的代理
        using children_view = details::children_view<node_proxy>;
        using children_const_view = details::children_view<node_const_proxy>;
        using children_iterator = details::children_iterator<node_proxy>;
        using children_const_iterator = details::children_iterator<node_const_proxy>;

        trees() = default;
        explicit trees(const Alloc& a) noexcept : alloc(a) {}

        // 拷贝构造：深拷贝 other 的所有树。
        // 分配器按 select_on_container_copy_construction 选择。
        trees(const trees& other)
            : alloc(std::allocator_traits<Alloc>::select_on_container_copy_construction(other.alloc))
        {
            auto guard = make_scope_guard([this]() noexcept { clear(); });
            copy_from(other);
            guard.dismiss();
        }

        // 拷贝赋值：先在临时森林里完整复制，成功后再替换自己（强异常安全）。
        trees& operator=(const trees& other)
        {
            if (this == &other)
            {
                return *this;
            }
            Alloc target_alloc = alloc;
            if constexpr (std::allocator_traits<Alloc>::propagate_on_container_copy_assignment::value)
            {
                target_alloc = other.alloc;
            }
            trees tmp{ target_alloc };
            tmp.copy_from(other);
            *this = std::move(tmp);
            return *this;
        }

        // 从外部树形结构递归构造整片森林：roots 是根句柄范围，
        // 每个句柄都必须有效（本接口不做判空），按范围顺序建树。
        template<class ValueGetter, class ChildrenGetter, class RangeOfHandles>
        trees(ValueGetter vg, ChildrenGetter cg, RangeOfHandles roots, const Alloc& a = {})
            : alloc(a)
        {
            auto guard = make_scope_guard([this]() noexcept { clear(); });
            for (auto&& h : roots)
            {
                build_tree(*this, vg, cg, h);
            }
            guard.dismiss();
        }

        // 内嵌哨兵让 basic_intrusive_list 不可移动，所以 move 必须手写：
        // 整链 splice 就是一次 O(1) 的"搬家"，源森林被复位为空。
        trees(trees&& other) noexcept(std::is_nothrow_move_constructible_v<Alloc>)
            : alloc(std::move(other.alloc))
        {
            roots.splice(roots.end(), other.roots);
        }

        trees& operator=(trees&& other) noexcept(std::is_nothrow_move_assignable_v<Alloc>)
        {
            if (this != &other)
            {
                clear();                            // 先用旧分配器销毁自己的节点
                alloc = std::move(other.alloc);     // 接管对方的分配器
                roots.splice(roots.end(), other.roots);
            }
            return *this;
        }

        ~trees()
        {
            clear();
        }

        Alloc get_allocator() const noexcept
        {
            return alloc;
        }

        bool empty() const noexcept
        {
            return roots.empty();
        }

        // 森林入口：根链的 begin / end。遍历顺序就是树的插入顺序。
        // 命名用 begin_tree/end_tree 而不是 begin/end：森林遍历的是"树"，
        // 每棵树内部再用 node_proxy 的 child_begin()/child_end() 下钻。
        // 空森林时 begin_tree() == end_tree()。
        node_proxy begin_tree() noexcept
        {
            return node_proxy{ roots.begin() };
        }
        node_proxy end_tree() noexcept
        {
            return node_proxy{ roots.end() };
        }
        node_const_proxy begin_tree() const noexcept
        {
            return node_const_proxy{ roots.begin() };
        }
        node_const_proxy end_tree() const noexcept
        {
            return node_const_proxy{ roots.end() };
        }

        // 森林的所有根节点（每棵树是一棵子树的根）；视图元素是 node_proxy
        children_view all_children() noexcept
        {
            return children_view{ begin_tree(), end_tree() };
        }
        children_const_view all_children() const noexcept
        {
            return children_const_view{ begin_tree(), end_tree() };
        }

        // 对根链（森林的顶层“孩子”）按值稳定排序；比较器接收 const T&。
        template<class Compare>
        void sort_children(Compare comp)
        {
            roots.sort([&comp](const list_type::node_type& a, const list_type::node_type& b)
            {
                return std::invoke(comp,
                    static_cast<const node_type&>(a).value(),
                    static_cast<const node_type&>(b).value());
            });
        }

        // 在 pos 的左侧（前面）插入一个新节点，返回新节点的代理。
        // pos 可以是任意兄弟链上的节点，也可以是 end 位置：
        //   emplace(end_tree(), v)              在森林末尾追加一棵树
        //   emplace(parent.child_end(), v)      给 parent 追加一个孩子
        //   emplace(x, v)                       在 x 左边插入一个兄弟
        // 插入只依赖 pos 所在节点的 prev/next，不需要知道它属于哪条链，
        // 所以不需要父指针。
        template<class... Args>
        node_proxy emplace(node_proxy pos, Args&&... args)
        {
            node_type* p = make_node(std::forward<Args>(args)...);
            auto it = list_type::insert(pos.base(), *p);
            return node_proxy{ it };
        }

        node_proxy insert(node_proxy pos, const T& value)
        {
            return emplace(pos, value);
        }
        node_proxy insert(node_proxy pos, T&& value)
        {
            return emplace(pos, std::move(value));
        }

        // 删除 p 指向的节点及其整棵子树。
        // p 可以从任意兄弟链来：release() 只认节点自己的 prev/next，
        // 因此不关心它挂在根链还是某个父节点的孩子链上。
        void erase(node_proxy p) noexcept
        {
            node_type& n = static_cast<node_type&>(*p.base());

            // 用节点自己的 children 头当工作链：先把孩子整链搬走，
            // 再销毁父节点，然后逐层把孙子们搬进工作链。
            // 额外空间 O(1)，销毁顺序是父先子后。
            list_type work;
            work.splice(work.end(), n.children);
            p.base()->release();
            destroy_node(n);
            while (!work.empty())
            {
                auto it = work.begin();
                node_type& cur = static_cast<node_type&>(*it);
                work.erase(it);
                work.splice(work.end(), cur.children);
                destroy_node(cur);
            }
        }

        // 删除 [first, last) 里的每一棵子树（同一条兄弟链上的连续范围）。
        // first == last 时无操作；last 可以是 end_tree()/child_end()。
        void erase(node_proxy first, node_proxy last) noexcept
        {
            while (first != last)
            {
                node_proxy next = first;
                ++next;
                erase(first);
                first = next;
            }
        }

        // 把 p 指向的子树整棵摘下来，成为新森林里的一棵树。
        // 结构操作是 O(1)（返回值的 move 也是 O(1)）。
        trees cut(node_proxy p) noexcept
        {
            trees out{ alloc };
            // push_back 内部的 insert 会先 release：自动从原来的根链/孩子链摘除
            out.roots.push_back(*p.base());
            return out;
        }

        // 把 [first, last) 这段兄弟（连同各自的子树）整段摘下来，成为新森林；
        // first == last 时得到空森林。last 可以是 end_tree()/child_end()。
        // 要求 first、last 在同一条兄弟链上，且 first 在 last 之前。
        trees cut(node_proxy first, node_proxy last) noexcept
        {
            trees out{ alloc };
            // 第二个参数（源链）在链表实现里不使用：区间端点自带 prev/next，
            // 这里传 roots 只为满足与 std::list::splice 一致的签名。
            list_type::splice(out.roots.end(), roots, first.base(), last.base());
            return out;
        }

        // 把 parent 的全部孩子整链摘下来，成为新森林；parent 变成叶子。
        // 等价于 cut(parent.child_begin(), parent.child_end())。
        trees cut_children(node_proxy parent) noexcept
        {
            trees out{ alloc };
            node_type& p = static_cast<node_type&>(*parent.base());
            out.roots.splice(out.roots.end(), p.children);
            return out;
        }

        // 把整片森林 sub 插到 pos 之前；sub 变空。O(1)。
        // 要求 pos 不在 sub 里；跨森林搬运要求两边的分配器能互相释放（相等）。
        void splice(node_proxy pos, trees&& sub) noexcept
        {
            if (this == &sub || sub.empty())
            {
                return;
            }
            list_type::splice(pos.base(), sub.roots, sub.roots.begin(), sub.roots.end());
        }

        // 把 node 这棵子树搬到 pos 之前（同链重排或跨父重挂）。O(1)。
        // 要求 node != pos，且 pos 不在 node 的子树里。
        void splice(node_proxy pos, node_proxy node) noexcept
        {
            if (pos == node)
            {
                return;
            }
            list_type::insert(pos.base(), *node.base());
        }

        // 把 [first, last) 这段兄弟搬到 pos 之前；first == last 时无操作。O(1)。
        // 要求 first/last 在同一条链上，pos 不在 [first, last) 里。
        void splice(node_proxy pos, node_proxy first, node_proxy last) noexcept
        {
            list_type::splice(pos.base(), roots, first.base(), last.base());
        }

        // 把 other 的整条根链追加到本森林末尾；other 变空。O(1)。
        // 跨森林搬运要求两边的分配器能互相释放（相等）。
        void merge(trees&& other) noexcept
        {
            if (this == &other || other.empty())
            {
                return;
            }
            roots.splice(roots.end(), other.roots);
        }

        // 交换两个节点在兄弟链上的位置（连同各自的孩子）。O(1)，纯指针。
        // 做法：在 l/r 前各插一个栈上占位记住位置，release 把两个节点摘链
        //（cut 的摘链动作），再 insert 到对方的占位前面，最后删掉占位。
        // 全程没有相邻判断，相邻/非相邻/同链/跨链走同一条路径；static，不需要分配器。
        // 注意：它不交换孩子，父子/祖先会成环，所以要求两者不互为祖先。
        // 允许 l/r 在不同森林：节点只是换了位置，所有权/分配器不变
        //（跨分配器释放的问题由调用方保证）。
        static void swap_sub_tree(node_proxy l, node_proxy r) noexcept
        {
            if (l == r)
            {
                return;
            }
            typename list_type::node_type dl, dr;             // 栈上占位，不承载 T
            auto pl = list_type::insert(l.base(), dl);        // 记住 l 的位置
            auto pr = list_type::insert(r.base(), dr);        // 记住 r 的位置
            l.base()->release();                              // 摘掉 l
            r.base()->release();                              // 摘掉 r
            list_type::insert(pr, *l.base());                 // l 放到 r 的位置
            list_type::insert(pl, *r.base());                 // r 放到 l 的位置
            pl.base()->release();                             // 删掉两个占位
            pr.base()->release();
        }

        // 交换两个节点的位置，并交换它们的两个子链。
        // 顺序：插占位 -> release 摘链（cut 的摘链动作）-> 交换孩子 -> 插回对方位置 -> 删占位。
        // 净效果：两个位置上的值互换、孩子留在原位、树形不变；O(1)，纯指针，不动 T。
        // 任意两个不同的有效节点都可以：相邻、同链、跨链、父子/祖先都成立。
        //（父子/祖先能成立的关键：交换孩子后，对方的占位节点会跟着孩子链
        //  落到对方子树里，回填时不会把自己插进自己的子树。）
        static void swap_node(node_proxy l, node_proxy r) noexcept
        {
            if (l == r)
            {
                return;
            }
            typename list_type::node_type dl, dr;
            auto pl = list_type::insert(l.base(), dl);
            auto pr = list_type::insert(r.base(), dr);
            l.base()->release();                              // 摘掉 l
            r.base()->release();                              // 摘掉 r

            // 交换两个孩子链
            node_type& na = static_cast<node_type&>(*l.base());
            node_type& nb = static_cast<node_type&>(*r.base());
            list_type tmp;
            tmp.splice(tmp.end(), na.children);
            na.children.splice(na.children.end(), nb.children);
            nb.children.splice(nb.children.end(), tmp);

            list_type::insert(pr, *l.base());                 // 拼回 r 的位置
            list_type::insert(pl, *r.base());                 // 拼回 l 的位置
            pl.base()->release();
            pr.base()->release();
        }

        // 递归写入：把句柄范围 hs 描述的整片森林插到 pos 之前（顺序按 hs 顺序）。
        //   vg(h) 取当前节点的值；cg(h) 返回孩子句柄的范围
        // 范围里的每个句柄都必须有效（本接口不做判空）。
        // 构造在临时森林里完成，抛异常时森林不变。
        template<class ValueGetter, class ChildrenGetter, class RangeOfHandles>
        void recur_and_write(node_proxy pos, ValueGetter vg, ChildrenGetter cg, RangeOfHandles hs)
        {
            trees tmp{ alloc };
            for (auto&& h : hs)
            {
                build_tree(tmp, vg, cg, h);
            }
            if (!tmp.empty())
            {
                list_type::splice(pos.base(), tmp.roots, tmp.roots.begin(), tmp.roots.end());
            }
        }

        // 递归遍历整片森林的每个元素（先序：父节点先于孩子，兄弟按链顺序）。
        // 与 std::for_each 一致：f 按值传入、按值返回，有状态的函数对象能把状态带出来。
        // f 不应改变森林结构（增删/摘接），但可以改元素的值。
        template<class F>
        F for_each(F f) noexcept(std::is_nothrow_invocable_v<F&, T&>&& std::is_nothrow_move_constructible_v<F>)
        {
            for (auto it = roots.begin(); it != roots.end(); ++it)
            {
                for_each_impl(static_cast<node_type&>(*it), f);
            }
            return f;
        }
        template<class F>
        F for_each(F f) const noexcept(std::is_nothrow_invocable_v<F&, const T&>&& std::is_nothrow_move_constructible_v<F>)
        {
            for (auto it = roots.begin(); it != roots.end(); ++it)
            {
                for_each_impl(static_cast<const node_type&>(*it), f);
            }
            return f;
        }

        // 销毁森林里的所有树
        void clear() noexcept
        {
            while (!roots.empty())
            {
                erase(node_proxy{ roots.begin() });
            }
        }

    private:
        // 在 dst 的根链末尾建一棵树（递归）；失败时把这棵已建的部分删掉。
        template<class ValueGetter, class ChildrenGetter, class InitializeHandle>
        void build_tree(trees& dst, ValueGetter& vg, ChildrenGetter& cg, InitializeHandle h)
        {
            node_proxy root = dst.emplace(dst.end_tree(), std::invoke(vg, h));
            auto guard = make_scope_guard([&dst, root]() noexcept { dst.erase(root); });
            dst.write_children(root, vg, cg, h);
            guard.dismiss();
        }
        // 深拷贝一棵子树：根插到 pos 之前，再递归复制孩子。
        node_proxy copy_subtree(node_proxy pos, node_const_proxy src)
        {
            node_proxy dst = emplace(pos, *src);
            for (auto it = src.child_begin(); it != src.child_end(); ++it)
            {
                copy_subtree(dst.child_end(), it);
            }
            return dst;
        }
        // 把 other 的所有树复制到本森林末尾；本森林必须为空。
        void copy_from(const trees& other)
        {
            for (auto it = other.begin_tree(); it != other.end_tree(); ++it)
            {
                copy_subtree(end_tree(), it);
            }
        }
        // recur_and_write 的递归部分：先写本节点，再逐个孩子递归。
        // 孩子范围里的每个句柄都必须是有效句柄，这里不判空。
        template<class ValueGetter, class ChildrenGetter, class InitializeHandle>
        void write_children(node_proxy parent, ValueGetter& vg, ChildrenGetter& cg, InitializeHandle h)
        {
            for (auto&& child : std::invoke(cg, h))
            {
                node_proxy n = emplace(parent.child_end(), std::invoke(vg, child));
                write_children(n, vg, cg, child);
            }
        }
        // for_each 的递归部分；Node 是 node_type 或 const node_type。
        template<class Node, class F>
        static void for_each_impl(Node& n, F& f)
            noexcept(std::is_nothrow_invocable_v<F&, decltype(n.value())>)
        {
            std::invoke(f, n.value());
            for (auto it = n.children.begin(); it != n.children.end(); ++it)
            {
                for_each_impl(static_cast<Node&>(*it), f);
            }
        }
    };
}

namespace std::ranges
{
    // children_view 只借节点，不拥有节点，因此是 borrowed_range。
    template<class Proxy>
    inline constexpr bool enable_borrowed_range<Yc::details::children_view<Proxy>> = true;
}
