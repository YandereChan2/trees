#pragma once
#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>
namespace Yc
{
    template<class vptr = void*>
    class basic_intrusive_list;
    template<class vptr = void*>
    class basic_intrusive_list_iterator;
    template<class vptr = void*>
    class basic_intrusive_list_const_iterator;
    template<class vptr = void*>
    class intrusive_list_node_base
    {
    public:
        // Fancy pointer obtained from the template parameter vptr (a void pointer type)
        // through pointer_traits: its element type is this node type, not void.
        using node_pointer = typename std::pointer_traits<vptr>::template rebind<intrusive_list_node_base>;
        static_assert(std::is_same_v<typename std::pointer_traits<node_pointer>::element_type, intrusive_list_node_base>,
            "pointer_traits<vptr>::rebind must yield a pointer whose element_type is intrusive_list_node_base");
        static_assert(std::is_same_v<node_pointer, typename std::pointer_traits<node_pointer>::template rebind<intrusive_list_node_base>>,
            "rebinding node_pointer to the node type must yield node_pointer itself");
    private:
        node_pointer prev{ to_node_ptr(*this) };
        node_pointer next{ to_node_ptr(*this) };

        static node_pointer to_node_ptr(intrusive_list_node_base& node)noexcept
        {
            return std::pointer_traits<node_pointer>::pointer_to(node);
        }
        void connect_to(intrusive_list_node_base& next_node)noexcept
        {
            next = to_node_ptr(next_node);
            next_node.prev = to_node_ptr(*this);
        }
    public:
        intrusive_list_node_base() = default;
        // 节点的 prev/next 是自指的（空节点指向自己），逐位复制会让副本的
        // 哨位仍指向源对象，从而使链表结构静默损坏，因此禁止拷贝。
        // 节点本来就是就地构造、挂在链表里的对象，不支持搬迁语义。
        intrusive_list_node_base(const intrusive_list_node_base&) = delete;
        intrusive_list_node_base& operator=(const intrusive_list_node_base&) = delete;
        bool is_released()const noexcept
        {
            return prev == to_node_ptr(const_cast<intrusive_list_node_base&>(*this));
        }
        void release()noexcept
        {
            if (is_released())
            {
                return;
            }
            auto prev_p = prev;
            auto next_p = next;
            prev_p->connect_to(*next_p);
            connect_to(*this);
        }
        // 注意：这里的 friend 声明不能再写一遍 template<class vptr = void*>，
        // 那会把外层的 vptr 遮蔽掉（GCC 会直接报错）。
        template<class>
        friend class basic_intrusive_list;
        template<class>
        friend class basic_intrusive_list_iterator;
        template<class>
        friend class basic_intrusive_list_const_iterator;
    };

    template<class vptr>
    class basic_intrusive_list_iterator
    {
    public:
        using node_type = intrusive_list_node_base<vptr>;
        using node_ptr = typename std::pointer_traits<vptr>::template rebind<node_type>;
    private:
        node_ptr p{};
    public:
        basic_intrusive_list_iterator() = default;
        explicit basic_intrusive_list_iterator(node_ptr ptr)noexcept : p(ptr) {}
        auto base()const noexcept
        {
            return p;
        }
        // 迭代器指向的节点（basic_intrusive_list 的节点类型即 node_type）
        node_type& operator*()const noexcept
        {
            return *p;
        }
        node_type* operator->()const noexcept
        {
            return std::addressof(*p);
        }
        basic_intrusive_list_iterator& operator++()noexcept
        {
            p = (node_ptr)p->next;
            return *this;
        }
        basic_intrusive_list_iterator& operator--()noexcept
        {
            p = p->prev;
            return *this;
        }
        basic_intrusive_list_iterator operator++(int)noexcept
        {
            auto tmp = *this;
            ++*this;
            return tmp;
        }
        basic_intrusive_list_iterator operator--(int)noexcept
        {
            auto tmp = *this;
            --*this;
            return tmp;
        }
        friend bool operator==(basic_intrusive_list_iterator, basic_intrusive_list_iterator)noexcept = default;
        // 允许 iterator -> const_iterator
        operator basic_intrusive_list_const_iterator<vptr>()const noexcept
        {
            return basic_intrusive_list_const_iterator<vptr>{
                typename basic_intrusive_list_const_iterator<vptr>::node_ptr{ p } };
        }
        template<class U>
        friend class basic_intrusive_list;
    };

    // 常量迭代器：与 iterator 遍历逻辑相同，解引用返回 const 节点
    template<class vptr>
    class basic_intrusive_list_const_iterator
    {
    public:
        using node_type = intrusive_list_node_base<vptr>;
        using node_ptr = typename std::pointer_traits<vptr>::template rebind<const node_type>;
    private:
        node_ptr p{};
    public:
        basic_intrusive_list_const_iterator() = default;
        explicit basic_intrusive_list_const_iterator(node_ptr ptr)noexcept : p(ptr) {}
        auto base()const noexcept
        {
            return p;
        }
        const node_type& operator*()const noexcept
        {
            return *p;
        }
        const node_type* operator->()const noexcept
        {
            return std::addressof(*p);
        }
        basic_intrusive_list_const_iterator& operator++()noexcept
        {
            p = (node_ptr)p->next;
            return *this;
        }
        basic_intrusive_list_const_iterator& operator--()noexcept
        {
            p = p->prev;
            return *this;
        }
        basic_intrusive_list_const_iterator operator++(int)noexcept
        {
            auto tmp = *this;
            ++*this;
            return tmp;
        }
        basic_intrusive_list_const_iterator operator--(int)noexcept
        {
            auto tmp = *this;
            --*this;
            return tmp;
        }
        friend bool operator==(basic_intrusive_list_const_iterator, basic_intrusive_list_const_iterator)noexcept = default;
        template<class U>
        friend class basic_intrusive_list;
    };

    template<class vptr>
    class basic_intrusive_list
    {
    public:
        using node_type = intrusive_list_node_base<vptr>;
        using node_ptr = typename std::pointer_traits<vptr>::template rebind<node_type>;
        using const_node_ptr = typename std::pointer_traits<vptr>::template rebind<const node_type>;
        using iterator = basic_intrusive_list_iterator<vptr>;
        using const_iterator = basic_intrusive_list_const_iterator<vptr>;
    private:
        node_type sentinel;
        static node_ptr to_node_ptr(node_type& node)noexcept
        {
            return std::pointer_traits<node_ptr>::pointer_to(node);
        }
        static const_node_ptr to_const_node_ptr(const node_type& node)noexcept
        {
            return std::pointer_traits<const_node_ptr>::pointer_to(const_cast<node_type&>(node));
        }
    public:
        basic_intrusive_list() = default;
        iterator begin()noexcept
        {
            return iterator{ sentinel.next };
        }
        iterator end()noexcept
        {
            return iterator{ to_node_ptr(sentinel) };
        }
        // 链表自身的哨兵节点（此链表所有元素的末尾都指向它）
        node_type& sentinel_node()noexcept
        {
            return sentinel;
        }
        const node_type& sentinel_node()const noexcept
        {
            return sentinel;
        }
        // 摘掉链表中所有元素（元素本身不销毁）：每个元素回到"空节点"自指状态。
        // 注意不能边遍历边对元素调用 release()——release 会改写邻居指针，
        // 使链表哨兵的 next 又指回链内，遍历就会来回转圈。
        void clear()noexcept
        {
            node_type* p = std::to_address(sentinel.next);
            // 每一步先取 next 再断开 p，避免读到已被改写的指针；
            // 同时对"指针为 null"做保护（链表被外部破坏时不至于崩）。
            while (p != nullptr && p != &sentinel)
            {
                node_type* next = std::to_address(p->next);
                p->connect_to(*p);          // 自指：等同于摘链后释放
                if (next == p)
                {
                    break;                  // 自指说明已断开，防止绕圈
                }
                p = next;
            }
            sentinel.connect_to(sentinel);  // 链表复位为空
        }
        // 常量遍历：桶在 const 语境下也能取出迭代器
        const_iterator begin()const noexcept
        {
            return const_iterator{ to_const_node_ptr(sentinel.next ? *sentinel.next : sentinel) };
        }
        const_iterator end()const noexcept
        {
            return const_iterator{ to_const_node_ptr(sentinel) };
        }
        bool empty()const noexcept
        {
            return sentinel.is_released();
        }
        node_type& front()noexcept
        {
            return *begin().base();
        }
        node_type& back()noexcept
        {
            auto it = end();
            --it;
            return *it.base();
        }
        // 在 pos 之前插入一个已经构造好的节点。静态函数：挂接只需要
        // pos 所指节点的 prev/next，不需要链对象本身，因此 pos 可以属于
        // 任意一条兄弟链（包括该链的 end()）。
        // trees 这类没有父指针的结构用它实现"在节点左侧插入兄弟"。
        static iterator insert(iterator pos, node_type& node) noexcept
        {
            node.release();
            auto target = pos.base();
            target->prev->connect_to(node);
            node.connect_to(*target);
            return iterator{ to_node_ptr(node) };
        }
        iterator erase(iterator pos)
        {
            auto next = pos.base()->next;
            pos.base()->release();
            return iterator{ next };
        }
        void push_front(node_type& node)
        {
            insert(begin(), node);
        }
        void push_back(node_type& node)
        {
            insert(end(), node);
        }
        void pop_front()
        {
            erase(begin());
        }
        void pop_back()
        {
            auto it = end();
            --it;
            erase(it);
        }
        void splice(iterator pos, basic_intrusive_list& other)
        {
            if (other.empty() || &other == this)
                return;
            auto target = pos.base();
            auto first = other.sentinel.next;
            auto last = other.sentinel.prev;
            target->prev->connect_to(*first);
            last->connect_to(*target);
            other.sentinel.connect_to(other.sentinel);
        }
        void splice(iterator pos, basic_intrusive_list&& other)
        {
            splice(pos, other);
        }
        static void splice(iterator pos, basic_intrusive_list&, iterator it)
        {
            auto node = it.base();
            auto target = pos.base();
            if (node == target)
                return;
            node->release();
            target->prev->connect_to(*node);
            node->connect_to(*target);
        }
        static void splice(iterator pos, basic_intrusive_list&& other, iterator it)
        {
            splice(pos, other, it);
        }
        // 把 [first, last) 这一段兄弟链搬到 pos 之前。静态函数：
        // 挂接只用到 pos/first/last 所指节点的 prev/next，不需要链对象本身。
        // 第二个参数与 std::list::splice 的签名保持一致，实现不使用它。
        // 调用方保证 first/last 来自同一条链、pos 不在 [first, last) 内。
        static void splice(iterator pos, basic_intrusive_list&, iterator first, iterator last)
        {
            if (first == last)
                return;
            auto range_first = first.base();
            auto it = last;
            --it;
            auto range_last = it.base();
            auto after = last.base();
            range_first->prev->connect_to(*after);
            auto target = pos.base();
            target->prev->connect_to(*range_first);
            range_last->connect_to(*target);
        }
        static void splice(iterator pos, basic_intrusive_list&& other, iterator first, iterator last)
        {
            splice(pos, other, first, last);
        }

        // ===================================================================
        // 排序 / 合并 / 删除
        //
        // 比较器和谓词接收 const node_type&（节点基类）；用户可 static_cast
        // 到自己的派生节点再取 value()。
        // remove_if / unique 的 deleter 接收 node_type&，在节点已经 release()
        // 之后调用，负责析构/释放节点；返回实际删除的节点数。
        // ===================================================================
        template<class Compare>
        void sort(Compare comp)
        {
            if (empty())
            {
                return;
            }
            auto second = begin();
            ++second;
            if (second == end())
            {
                return;
            }

            // 自底向上归并：bin[i] 保存长度 2^i 的有序段，O(log n) 个链表，无分配。
            constexpr std::size_t max_bins = 64;
            basic_intrusive_list bins[max_bins];
            basic_intrusive_list run;

            // 比较器抛异常时把 bins/run 里的节点全部拼回 *this，
            // 保证链表仍然完整（顺序不保证），不会把节点留在局部链表里。
            struct rollback_guard
            {
                basic_intrusive_list* self;
                basic_intrusive_list* bins;
                basic_intrusive_list* run;
                bool active = true;
                ~rollback_guard()
                {
                    if (!active)
                    {
                        return;
                    }
                    for (std::size_t i = 0; i != 64; ++i)
                    {
                        if (!bins[i].empty())
                        {
                            self->splice(self->end(), bins[i]);
                        }
                    }
                    if (!run->empty())
                    {
                        self->splice(self->end(), *run);
                    }
                }
            } guard{ this, bins, &run };

            while (!empty())
            {
                run.splice(run.end(), *this, begin());   // 取一个节点作为长度 1 的段
                std::size_t i = 0;
                while (i != max_bins && !bins[i].empty())
                {
                    bins[i].merge(run, comp);            // 相等时取 bins[i]，保持稳定
                    run.splice(run.end(), bins[i]);      // 合并结果搬到 run
                    ++i;
                }
                if (i == max_bins)
                {
                    break;                               // 2^64 个节点，实际不会到这里
                }
                bins[i].splice(bins[i].end(), run);
            }
            if (!run.empty())
            {
                merge(run, comp);
            }

            // 高位 bin 是更早的段，从高到低合并才能保持稳定。
            for (std::size_t i = max_bins; i != 0;)
            {
                --i;
                if (!bins[i].empty())
                {
                    merge(bins[i], comp);
                }
            }
            guard.active = false;
        }

        // 把有序的 other 稳定合并进有序的 *this；other 变空。
        template<class Compare>
        void merge(basic_intrusive_list& other, Compare comp)
        {
            if (&other == this || other.empty())
            {
                return;
            }
            auto first1 = begin();
            auto first2 = other.begin();
            while (first1 != end() && first2 != other.end())
            {
                if (comp(*first2, *first1))
                {
                    auto next2 = first2;
                    ++next2;
                    splice(first1, other, first2);        // 把 other 的节点移到 first1 前
                    first2 = next2;
                }
                else
                {
                    ++first1;                             // 相等时先保留 *this，稳定
                }
            }
            if (first2 != other.end())
            {
                splice(end(), other, first2, other.end());
            }
        }
        template<class Compare>
        void merge(basic_intrusive_list&& other, Compare comp)
        {
            merge(other, comp);
        }

        template<class Pred, class Deleter>
        std::size_t remove_if(Pred pred, Deleter del)
        {
            std::size_t removed = 0;
            auto it = begin();
            while (it != end())
            {
                auto next = it;
                ++next;
                if (pred(*it))
                {
                    node_type& node = *it;
                    node.release();
                    del(node);
                    ++removed;
                }
                it = next;
            }
            return removed;
        }

        template<class BinaryPred, class Deleter>
        std::size_t unique(BinaryPred pred, Deleter del)
        {
            std::size_t removed = 0;
            if (empty())
            {
                return 0;
            }
            auto cur = begin();
            auto next = cur;
            ++next;
            while (next != end())
            {
                if (pred(*cur, *next))
                {
                    node_type& node = *next;
                    auto after = next;
                    ++after;
                    node.release();
                    del(node);
                    ++removed;
                    next = after;
                }
                else
                {
                    cur = next;
                    ++next;
                }
            }
            return removed;
        }
    };

    // 带数据的侵入式链表节点：既可以被 basic_intrusive_list 串起来，
    // 又存放用户数据 T。数据用 union 包裹，便于由分配器分别负责构造/析构。
    template<class vptr, class T>
    class intrusive_list_node : public intrusive_list_node_base<vptr>
    {
        union data_t
        {
            T data;
            data_t() {}
            ~data_t() {}
        }d;
    public:
        // 注意：必须有这个显式默认构造。否则 allocator_traits::construct 走
        // 的是隐式默认构造，它不会调用基类 intrusive_list_node_base 的
        // NSDMI（prev/next 保持垃圾值），挂链时就会踩野指针。
        intrusive_list_node() = default;
        template<class Alloc, class... Args>
        void construct_value(Alloc& alloc, Args&&... args)noexcept(std::is_nothrow_constructible_v<T, Args...>)
        {
            std::allocator_traits<Alloc>::construct(alloc, std::addressof(d.data), std::forward<Args>(args)...);
        }
        template<class Alloc>
        void destroy_value(Alloc& alloc)noexcept
        {
            std::allocator_traits<Alloc>::destroy(alloc, std::addressof(d.data));
        }
        T& value()noexcept
        {
            return d.data;
        }
        const T& value()const noexcept
        {
            return d.data;
        }
    };
}
