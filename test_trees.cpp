// trees.h 的最小自检 / 用法示例。
//
// Build with MSVC:
//   cl /nologo /std:c++latest /EHsc /utf-8 /I . test_trees.cpp
//
// 覆盖：建树、根链遍历、孩子链遍历、代理的 ++/--/解引用、
//       erase 子树、cut 子树、move、const 遍历、节点生命周期。
#include "trees.h"

#include <array>
#include <cassert>
#include <cstdio>
#include <functional>
#include <iterator>
#include <ranges>
#include <type_traits>
#include <utility>
#include <vector>

static_assert(std::bidirectional_iterator<Yc::trees<int>::node_proxy>);
static_assert(std::bidirectional_iterator<Yc::trees<int>::node_const_proxy>);

namespace
{
    template<class Iter>
    std::vector<int> collect(Iter first, Iter last)
    {
        std::vector<int> out;
        for (; first != last; ++first)
        {
            out.push_back(*first);
        }
        return out;
    }

    // 统计节点生命周期：每构造一个 T，alive +1；析构 -1。
    struct tracked
    {
        static inline int alive = 0;
        int v = 0;

        explicit tracked(int x = 0) : v(x) { ++alive; }
        tracked(const tracked& o) : v(o.v) { ++alive; }
        tracked(tracked&& o) noexcept : v(o.v) { ++alive; }
        ~tracked() { --alive; }
    };

    // 可以在构造/拷贝时抛异常的类型，用来验证 RAII 回滚
    struct throwing_t
    {
        static inline int alive = 0;
        static inline int copies_before_throw = -1;
        int v = 0;

        explicit throwing_t(int x) : v(x)
        {
            if (x == 99)
            {
                throw 1;
            }
            ++alive;
        }
        throwing_t(const throwing_t& o) : v(o.v)
        {
            if (copies_before_throw == 0)
            {
                throw 1;
            }
            if (copies_before_throw > 0)
            {
                --copies_before_throw;
            }
            ++alive;
        }
        throwing_t(throwing_t&& o) noexcept : v(o.v) { ++alive; }
        ~throwing_t() { --alive; }
    };
}

int main()
{
    static_assert(std::is_same_v<Yc::trees<int>::pointer, int*>);
    static_assert(std::is_same_v<Yc::trees<int>::const_pointer, const int*>);
    static_assert(std::is_same_v<Yc::trees<int>::void_pointer, void*>);
    static_assert(std::is_same_v<Yc::trees<int>::const_void_pointer, const void*>);
    static_assert(std::is_same_v<Yc::trees<int>::reference, int&>);
    static_assert(std::is_same_v<Yc::trees<int>::const_reference, const int&>);

    // ---- 结构 ----
    {
        Yc::trees<int> f;
        assert(f.empty());
        assert(f.begin_tree() == f.end_tree());

        auto r1 = f.emplace(f.end_tree(), 1);          // 第一棵树
        auto r2 = f.emplace(f.end_tree(), 2);          // 第二棵树
        auto c11 = f.emplace(r1.child_end(), 11);
        auto c12 = f.emplace(r1.child_end(), 12);
        auto c21 = f.emplace(r2.child_end(), 21);
        auto c111 = f.emplace(c11.child_end(), 111);

        // 根链
        assert((collect(f.begin_tree(), f.end_tree()) == std::vector<int>{1, 2}));

        // 从第一棵树下钻到它的孩子链
        assert((collect(f.begin_tree().child_begin(), f.begin_tree().child_end())
            == std::vector<int>{11, 12}));

        // 代理下钻
        assert((collect(r1.child_begin(), r1.child_end()) == std::vector<int>{11, 12}));
        assert((collect(r2.child_begin(), r2.child_end()) == std::vector<int>{21}));
        assert((collect(c11.child_begin(), c11.child_end()) == std::vector<int>{111}));
        assert(c12.child_begin() == c12.child_end());       // 叶子

        // 代理的 ++ / -- / 后置 / ->
        auto it = r1.child_begin();
        assert(*it == 11);
        ++it;
        assert(*it == 12);
        --it;
        assert(*it == 11);
        auto post = it++;
        assert(*post == 11);
        assert(*it == 12);
        auto post2 = it--;
        assert(*post2 == 12);
        assert(*it == 11);
        assert(*it.operator->() == 11);

        // 根链也能反向走
        auto last = f.end_tree();
        --last;
        assert(*last == 2);

        // 删除一棵子树：c11 与它下面的 111 一起消失
        f.erase(c11);
        assert((collect(r1.child_begin(), r1.child_end()) == std::vector<int>{12}));

        // 切下第二棵树
        auto sub = f.cut(r2);
        assert((collect(f.begin_tree(), f.end_tree()) == std::vector<int>{1}));
        assert((collect(sub.begin_tree(), sub.end_tree()) == std::vector<int>{2}));
        assert((collect(sub.begin_tree().child_begin(), sub.begin_tree().child_end())
            == std::vector<int>{21}));

        // const 遍历 + node_proxy -> node_const_proxy
        const Yc::trees<int>& cf = f;
        assert((collect(cf.begin_tree(), cf.end_tree()) == std::vector<int>{1}));
        assert((collect(cf.begin_tree().child_begin(), cf.begin_tree().child_end())
            == std::vector<int>{12}));
        Yc::trees<int>::node_const_proxy ci = f.begin_tree();
        assert(ci == f.begin_tree());
        assert(*ci == 1);

        f.clear();
        assert(f.empty());
    }

    // ---- 删除兄弟链中间/首/尾的孩子 ----
    {
        Yc::trees<int> g;
        auto root = g.emplace(g.end_tree(), 0);
        auto a = g.emplace(root.child_end(), 1);
        auto b = g.emplace(root.child_end(), 2);
        auto c = g.emplace(root.child_end(), 3);

        g.erase(b);
        assert((collect(root.child_begin(), root.child_end()) == std::vector<int>{1, 3}));
        g.erase(a);
        assert((collect(root.child_begin(), root.child_end()) == std::vector<int>{3}));
        g.erase(c);
        assert(root.child_begin() == root.child_end());

        g.emplace(g.end_tree(), 10);
        g.emplace(g.end_tree(), 20);
        g.clear();
        assert(g.empty());
    }

    // ---- 第一棵树没有孩子时，孩子链的 begin == end ----
    {
        Yc::trees<int> g;
        g.emplace(g.end_tree(), 7);
        assert(g.begin_tree().child_begin() == g.begin_tree().child_end());
        assert(g.begin_tree() != g.end_tree());
        assert(*g.begin_tree() == 7);
    }

    // ---- 切下一个非根节点：它带着自己的子树成为新森林 ----
    {
        Yc::trees<int> g;
        auto r = g.emplace(g.end_tree(), 1);
        auto a = g.emplace(r.child_end(), 2);
        g.emplace(a.child_end(), 3);

        auto sub = g.cut(a);
        assert((collect(g.begin_tree(), g.end_tree()) == std::vector<int>{1}));
        assert(r.child_begin() == r.child_end());
        assert((collect(sub.begin_tree(), sub.end_tree()) == std::vector<int>{2}));
        assert((collect(sub.begin_tree().child_begin(), sub.begin_tree().child_end())
            == std::vector<int>{3}));
    }

    // ---- insert/emplace：在 pos 左侧（前面）插入一个兄弟 ----
    {
        Yc::trees<int> g;
        auto r1 = g.emplace(g.end_tree(), 1);          // 根链追加
        auto r2 = g.emplace(g.end_tree(), 2);
        auto a = g.emplace(r1.child_end(), 11);        // 孩子链追加
        auto b = g.emplace(r1.child_end(), 12);
        (void)r2;
        (void)b;

        auto x = g.insert(a, 10);                      // 在 11 左边插 10
        assert(*x == 10);
        assert((collect(r1.child_begin(), r1.child_end()) == std::vector<int>{10, 11, 12}));

        g.insert(r1.child_begin(), 9);                 // 插到最左
        g.insert(r1.child_end(), 13);                  // 插到最右（= 追加）
        assert((collect(r1.child_begin(), r1.child_end())
            == std::vector<int>{9, 10, 11, 12, 13}));

        const int cv = 7;
        g.insert(g.end_tree(), cv);                    // const T& 重载
        g.insert(g.begin_tree(), 0);                   // 插到根链最前
        g.emplace(g.end_tree(), 8);                    // emplace 追加一棵树
        assert((collect(g.begin_tree(), g.end_tree()) == std::vector<int>{0, 1, 2, 7, 8}));

        auto leaf = g.emplace(g.end_tree(), 14);
        assert(leaf.child_begin() == leaf.child_end()); // 新节点没有孩子

        Yc::trees<int> h;
        auto only = h.insert(h.end_tree(), 42);        // 空森林的第一次插入
        assert(h.begin_tree() == only);
        assert(*h.begin_tree() == 42);
        h.insert(h.begin_tree(), 41);                  // 插到唯一一棵树左边
        assert((collect(h.begin_tree(), h.end_tree()) == std::vector<int>{41, 42}));
    }


    // ---- cut 的范围版本 / cut_children ----
    {
        Yc::trees<int> f;
        auto r1 = f.emplace(f.end_tree(), 1);
        auto r2 = f.emplace(f.end_tree(), 2);
        auto a = f.emplace(r1.child_end(), 10);
        auto b = f.emplace(r1.child_end(), 11);
        auto c = f.emplace(r1.child_end(), 12);
        auto d = f.emplace(r1.child_end(), 13);
        auto leaf = f.emplace(b.child_end(), 111);   // 子树跟着被摘的节点走
        (void)r2;
        (void)a;
        (void)d;
        (void)leaf;

        // 半开区间 [b, c) = {11}
        auto one = f.cut(b, c);
        assert((collect(one.begin_tree(), one.end_tree()) == std::vector<int>{11}));
        assert((collect(one.begin_tree().child_begin(), one.begin_tree().child_end())
            == std::vector<int>{111}));
        assert((collect(r1.child_begin(), r1.child_end()) == std::vector<int>{10, 12, 13}));

        // [begin, end) 摘走整条孩子链
        auto rest = f.cut(r1.child_begin(), r1.child_end());
        assert((collect(rest.begin_tree(), rest.end_tree()) == std::vector<int>{10, 12, 13}));
        assert(r1.child_begin() == r1.child_end());

        // cut_children 等价于整条孩子链
        f.emplace(r1.child_end(), 20);
        f.emplace(r1.child_end(), 21);
        auto kids = f.cut_children(r1);
        assert((collect(kids.begin_tree(), kids.end_tree()) == std::vector<int>{20, 21}));
        assert(r1.child_begin() == r1.child_end());

        // 提取根链的一段
        auto tail = f.cut(r2, f.end_tree());
        assert((collect(tail.begin_tree(), tail.end_tree()) == std::vector<int>{2}));
        assert((collect(f.begin_tree(), f.end_tree()) == std::vector<int>{1}));

        // 空范围得到空森林
        auto none = f.cut(f.begin_tree(), f.begin_tree());
        assert(none.empty());
        assert((collect(f.begin_tree(), f.end_tree()) == std::vector<int>{1}));
    }

    // ---- splice / merge / erase 范围 ----
    {
        Yc::trees<int> f;
        auto r1 = f.emplace(f.end_tree(), 1);
        auto r2 = f.emplace(f.end_tree(), 2);
        auto a = f.emplace(r1.child_end(), 11);
        auto b = f.emplace(r1.child_end(), 12);
        auto c = f.emplace(r1.child_end(), 13);
        (void)a;
        (void)b;
        (void)c;

        // 整片森林回接：cut 出 r1 的孩子，再 splice 到 r2 下
        auto kids = f.cut_children(r1);
        assert((collect(kids.begin_tree(), kids.end_tree()) == std::vector<int>{11, 12, 13}));
        f.splice(r2.child_end(), std::move(kids));
        assert(kids.empty());
        assert(r1.child_begin() == r1.child_end());
        assert((collect(r2.child_begin(), r2.child_end()) == std::vector<int>{11, 12, 13}));

        // 单棵子树搬移：把 12 从 r2 搬到 r1
        auto twelve = r2.child_begin();
        ++twelve;                                  // 11 -> 12
        f.splice(r1.child_end(), twelve);
        assert((collect(r1.child_begin(), r1.child_end()) == std::vector<int>{12}));
        assert((collect(r2.child_begin(), r2.child_end()) == std::vector<int>{11, 13}));

        // 范围搬移：[11, 13) = {11} 搬到 12 左边
        auto first = r2.child_begin();             // 11
        auto last = first;
        ++last;                                    // 13
        f.splice(r1.child_begin(), first, last);
        assert((collect(r1.child_begin(), r1.child_end()) == std::vector<int>{11, 12}));
        assert((collect(r2.child_begin(), r2.child_end()) == std::vector<int>{13}));

        // merge：把另一片森林追加到末尾
        Yc::trees<int> g;
        g.emplace(g.end_tree(), 21);
        g.emplace(g.end_tree(), 22);
        f.merge(std::move(g));
        assert(g.empty());
        assert((collect(f.begin_tree(), f.end_tree()) == std::vector<int>{1, 2, 21, 22}));

        // 整片森林 splice：cut 下 r2，再插回最前面
        auto sub = f.cut(r2);
        assert((collect(sub.begin_tree(), sub.end_tree()) == std::vector<int>{2}));
        f.splice(f.begin_tree(), std::move(sub));
        assert(sub.empty());
        assert((collect(f.begin_tree(), f.end_tree()) == std::vector<int>{2, 1, 21, 22}));
        assert((collect(f.begin_tree().child_begin(), f.begin_tree().child_end())
            == std::vector<int>{13}));

        // erase 范围：删掉前两棵树（2 和 1）
        auto rf = f.begin_tree();
        auto rl = rf;
        ++rl;
        ++rl;                                      // 21
        f.erase(rf, rl);
        assert((collect(f.begin_tree(), f.end_tree()) == std::vector<int>{21, 22}));

        // erase 单棵
        f.erase(f.begin_tree());
        assert((collect(f.begin_tree(), f.end_tree()) == std::vector<int>{22}));
    }

    // ---- swap_sub_tree / swap_node ----
    {
        Yc::trees<int> f;
        auto r = f.emplace(f.end_tree(), 1);
        auto a = f.emplace(r.child_end(), 2);
        auto b = f.emplace(r.child_end(), 3);
        auto a1 = f.emplace(a.child_end(), 21);
        (void)a1;

        // 相邻兄弟交换：2 和 3 互换位置，21 跟着 2 走
        Yc::trees<int>::swap_sub_tree(a, b);
        assert((collect(r.child_begin(), r.child_end()) == std::vector<int>{3, 2}));
        auto second = r.child_begin();
        ++second;
        assert(*second == 2);
        assert((collect(second.child_begin(), second.child_end()) == std::vector<int>{21}));
        assert(r.child_begin().child_begin() == r.child_begin().child_end());

        // 换回来
        Yc::trees<int>::swap_sub_tree(a, b);
        assert((collect(r.child_begin(), r.child_end()) == std::vector<int>{2, 3}));
        assert((collect(a.child_begin(), a.child_end()) == std::vector<int>{21}));

        // 非相邻根交换：1 <-> 20，1 的孩子跟着走
        auto r2 = f.emplace(f.end_tree(), 10);
        auto r3 = f.emplace(f.end_tree(), 20);
        (void)r2;
        Yc::trees<int>::swap_sub_tree(r, r3);
        assert((collect(f.begin_tree(), f.end_tree()) == std::vector<int>{20, 10, 1}));
        auto last = f.begin_tree();
        ++last;
        ++last;
        assert(*last == 1);
        assert((collect(last.child_begin(), last.child_end()) == std::vector<int>{2, 3}));

        // 换回来
        Yc::trees<int>::swap_sub_tree(r, r3);
        assert((collect(f.begin_tree(), f.end_tree()) == std::vector<int>{1, 10, 20}));

        // swap_node：节点换位 + 孩子链互换 => 值互换、孩子留在原位
        Yc::trees<int>::swap_node(a, b);
        assert((collect(r.child_begin(), r.child_end()) == std::vector<int>{3, 2}));
        auto first = r.child_begin();
        assert(*first == 3);
        assert((collect(first.child_begin(), first.child_end()) == std::vector<int>{21}));
        auto first2 = r.child_begin();
        ++first2;                                  // 2
        assert(first2.child_begin() == first2.child_end());

        // 换回来
        Yc::trees<int>::swap_node(a, b);
        assert((collect(r.child_begin(), r.child_end()) == std::vector<int>{2, 3}));
        assert((collect(a.child_begin(), a.child_end()) == std::vector<int>{21}));

        // 根之间：值互换，孩子留在原来的位置
        Yc::trees<int>::swap_node(r, r3);
        assert((collect(f.begin_tree(), f.end_tree()) == std::vector<int>{20, 10, 1}));
        assert((collect(f.begin_tree().child_begin(), f.begin_tree().child_end())
            == std::vector<int>{2, 3}));
        Yc::trees<int>::swap_node(r, r3);
        assert((collect(f.begin_tree(), f.end_tree()) == std::vector<int>{1, 10, 20}));
    }

    // ---- swap_sub_tree 穷举：5 个根的任意两两交换 ----
    {
        for (int i = 0; i < 5; ++i)
        {
            for (int j = i + 1; j < 5; ++j)
            {
                Yc::trees<int> t;
                std::vector<Yc::trees<int>::node_proxy> ps;
                for (int k = 0; k < 5; ++k)
                {
                    ps.push_back(t.emplace(t.end_tree(), k));
                }

                Yc::trees<int>::swap_sub_tree(ps[i], ps[j]);
                auto expected = std::vector<int>{0, 1, 2, 3, 4};
                std::swap(expected[i], expected[j]);
                assert((collect(t.begin_tree(), t.end_tree()) == expected));

                Yc::trees<int>::swap_sub_tree(ps[i], ps[j]);
                assert((collect(t.begin_tree(), t.end_tree())
                    == std::vector<int>{0, 1, 2, 3, 4}));
            }
        }
    }

    // ---- swap_node 跨父节点：值互换、孩子留在原位 ----
    {
        Yc::trees<int> g;
        auto p1 = g.emplace(g.end_tree(), 1);
        auto p2 = g.emplace(g.end_tree(), 2);
        auto a = g.emplace(p1.child_end(), 10);
        auto b = g.emplace(p2.child_end(), 20);
        auto a1 = g.emplace(a.child_end(), 100);
        (void)a1;

        g.swap_node(a, b);
        assert(*p1.child_begin() == 20);
        assert((collect(p1.child_begin().child_begin(), p1.child_begin().child_end())
            == std::vector<int>{100}));
        assert(*p2.child_begin() == 10);
        assert(p2.child_begin().child_begin() == p2.child_begin().child_end());

        g.swap_node(a, b);
        assert(*p1.child_begin() == 10);
        assert((collect(a.child_begin(), a.child_end()) == std::vector<int>{100}));
    }

    // ---- swap_node 父子（示意图场景） ----
    {
        Yc::trees<int> f;
        auto r = f.emplace(f.end_tree(), 1);
        auto a = f.emplace(r.child_end(), 2);
        auto b = f.emplace(r.child_end(), 3);
        auto a1 = f.emplace(a.child_end(), 21);
        (void)a1;

        Yc::trees<int>::swap_node(r, a);
        // 红(1) 与蓝(2) 换位：根变成 2，孩子是 [1, 3]，1 仍带着 21
        assert(*f.begin_tree() == 2);
        assert((collect(f.begin_tree().child_begin(), f.begin_tree().child_end())
            == std::vector<int>{1, 3}));
        auto c1 = f.begin_tree().child_begin();       // 1
        assert((collect(c1.child_begin(), c1.child_end()) == std::vector<int>{21}));
        auto c3 = f.begin_tree().child_begin();
        ++c3;                                         // 3
        assert(c3.child_begin() == c3.child_end());

        Yc::trees<int>::swap_node(r, a);                            // 换回来
        assert(*f.begin_tree() == 1);
        assert((collect(f.begin_tree().child_begin(), f.begin_tree().child_end())
            == std::vector<int>{2, 3}));
        assert((collect(a.child_begin(), a.child_end()) == std::vector<int>{21}));
    }

    // ---- swap_node 祖先/后代（隔代） ----
    {
        Yc::trees<int> f;
        auto r = f.emplace(f.end_tree(), 1);
        auto a = f.emplace(r.child_end(), 2);
        auto g = f.emplace(a.child_end(), 3);
        (void)g;

        Yc::trees<int>::swap_node(r, g);                            // 1 与 3 换位，2 留在中间
        assert(*f.begin_tree() == 3);
        assert((collect(f.begin_tree().child_begin(), f.begin_tree().child_end())
            == std::vector<int>{2}));
        auto mid = f.begin_tree().child_begin();      // 2
        assert((collect(mid.child_begin(), mid.child_end()) == std::vector<int>{1}));

        Yc::trees<int>::swap_node(r, g);                            // 换回来
        assert(*f.begin_tree() == 1);
        assert((collect(f.begin_tree().child_begin(), f.begin_tree().child_end())
            == std::vector<int>{2}));
        assert((collect(a.child_begin(), a.child_end()) == std::vector<int>{3}));
    }

    // ---- all_children：只遍历一层子树的根（借用视图） ----
    {
        Yc::trees<int> f;
        auto r = f.emplace(f.end_tree(), 1);
        auto a = f.emplace(r.child_end(), 2);
        auto b = f.emplace(r.child_end(), 3);
        f.emplace(a.child_end(), 21);
        f.emplace(f.end_tree(), 4);
        (void)b;

        // 森林的根
        std::vector<int> roots;
        for (auto p : f.all_children())
        {
            roots.push_back(*p);
        }
        assert((roots == std::vector<int>{1, 4}));

        // 节点的直接孩子
        std::vector<int> kids;
        for (auto p : r.all_children())
        {
            kids.push_back(*p);
        }
        assert((kids == std::vector<int>{2, 3}));

        // 公开的 children_view：两个代理构造 / 代理对构造
        using view_t = Yc::trees<int>::children_view;
        using cview_t = Yc::trees<int>::children_const_view;
        static_assert(std::is_same_v<decltype(f.all_children()), view_t>);
        static_assert(std::is_same_v<decltype(r.all_children()), view_t>);
        static_assert(std::is_same_v<decltype(std::declval<const Yc::trees<int>&>().all_children()),
            cview_t>);
        static_assert(std::is_same_v<decltype(std::declval<view_t&>().begin()),
            Yc::trees<int>::children_iterator>);

        view_t kv{ r.child_begin(), r.child_end() };
        std::vector<int> kv_vals;
        for (auto p : kv)
        {
            kv_vals.push_back(*p);
        }
        assert((kv_vals == std::vector<int>{2, 3}));

        view_t kv2{ std::pair{ r.child_begin(), r.child_end() } };
        std::vector<int> kv2_vals;
        for (auto p : kv2)
        {
            kv2_vals.push_back(*p);
        }
        assert(kv2_vals == kv_vals);

        // 孙辈：只走一层，不会下降
        auto two = r.child_begin();               // 2
        std::vector<int> grand;
        for (auto p : two.all_children())
        {
            grand.push_back(*p);
        }
        assert((grand == std::vector<int>{21}));

        // 叶子
        auto leaf = a.child_begin();              // 21
        assert(leaf.all_children().begin() == leaf.all_children().end());

        // 改值：只碰直接孩子
        for (auto p : r.all_children())
        {
            *p += 10;
        }
        assert((collect(r.child_begin(), r.child_end()) == std::vector<int>{12, 13}));
        assert(*a.child_begin() == 21);           // 孙辈没被碰

        // 视图直接喂给递归接口：cg 用 node_proxy::all_children
        auto vg = [](Yc::trees<int>::node_proxy p) -> int { return *p; };
        auto cg = [](Yc::trees<int>::node_proxy p) { return p.all_children(); };

        Yc::trees<int> copy{ vg, cg, f.all_children() };   // 根范围
        assert((collect(copy.begin_tree(), copy.end_tree()) == std::vector<int>{1, 4}));
        auto cr = copy.begin_tree();
        assert((collect(cr.child_begin(), cr.child_end()) == std::vector<int>{12, 13}));
        auto c12 = cr.child_begin();              // 12
        assert((collect(c12.child_begin(), c12.child_end()) == std::vector<int>{21}));
        auto c4 = copy.begin_tree();
        ++c4;
        assert(c4.child_begin() == c4.child_end());

        Yc::trees<int> g;
        g.recur_and_write(g.end_tree(), vg, cg, f.all_children());
        assert((collect(g.begin_tree(), g.end_tree()) == std::vector<int>{1, 4}));

        // const 视图
        const Yc::trees<int>& cf = f;
        std::vector<int> cvals;
        for (auto p : cf.all_children())
        {
            cvals.push_back(*p);
        }
        assert((cvals == std::vector<int>{1, 4}));

        std::vector<int> ckids;
        for (auto p : cf.begin_tree().all_children())
        {
            ckids.push_back(*p);
        }
        assert((ckids == std::vector<int>{12, 13}));

        // 空森林
        Yc::trees<int> e;
        assert(e.all_children().begin() == e.all_children().end());

        static_assert(std::ranges::view<decltype(f.all_children())>);
        static_assert(std::ranges::input_range<decltype(f.all_children())>);
        static_assert(std::ranges::bidirectional_range<decltype(f.all_children())>);
        static_assert(std::ranges::borrowed_range<decltype(f.all_children())>);
        static_assert(std::is_same_v<decltype(*f.all_children().begin()),
            Yc::trees<int>::node_proxy>);
    }

    // ---- RAII 异常回滚 ----
    {
        using T = throwing_t;
        Yc::trees<T> src;
        auto r = src.emplace(src.end_tree(), 1);
        src.emplace(r.child_end(), 2);
        assert(T::alive == 2);

        // T 构造抛：不泄漏
        bool threw = false;
        try
        {
            src.emplace(src.end_tree(), 99);
        }
        catch (int)
        {
            threw = true;
        }
        assert(threw);
        assert(T::alive == 2);
        assert(src.begin_tree()->v == 1);
        assert(src.begin_tree().child_begin()->v == 2);

        // 拷贝构造中途抛：已复制的部分由 guard 清掉
        T::copies_before_throw = 1;
        threw = false;
        try
        {
            Yc::trees<T> copy{ src };
        }
        catch (int)
        {
            threw = true;
        }
        assert(threw);
        assert(T::alive == 2);

        // 拷贝赋值中途抛：dst 保持原样
        Yc::trees<T> dst;
        dst.emplace(dst.end_tree(), 9);
        assert(T::alive == 3);
        T::copies_before_throw = 1;
        threw = false;
        try
        {
            dst = src;
        }
        catch (int)
        {
            threw = true;
        }
        assert(threw);
        assert(T::alive == 3);
        assert(dst.begin_tree()->v == 9);
        T::copies_before_throw = -1;
    }
    assert(throwing_t::alive == 0);

    // ---- sort_children ----
    {
        Yc::trees<int> f;
        auto r3 = f.emplace(f.end_tree(), 3);
        auto r1 = f.emplace(f.end_tree(), 1);
        auto r2 = f.emplace(f.end_tree(), 2);
        (void)r1;
        (void)r2;

        // 根链（森林的顶层孩子）
        f.sort_children(std::less<>{});
        assert((collect(f.begin_tree(), f.end_tree()) == std::vector<int>{1, 2, 3}));

        // 某节点的孩子链
        auto root = f.begin_tree();               // 1
        auto c3 = f.emplace(root.child_end(), 30);
        auto c1 = f.emplace(root.child_end(), 10);
        auto c2 = f.emplace(root.child_end(), 20);
        (void)c1;
        (void)c2;
        f.emplace(c3.child_end(), 99);            // 30 的孙子

        root.sort_children(std::less<>{});
        assert((collect(root.child_begin(), root.child_end())
            == std::vector<int>{10, 20, 30}));
        assert(*c3.child_begin() == 99);          // 只排一层，孙辈不动

        root.sort_children(std::greater<>{});
        assert((collect(root.child_begin(), root.child_end())
            == std::vector<int>{30, 20, 10}));

        // 空森林 / 单根
        Yc::trees<int> e;
        e.sort_children(std::less<>{});
        assert(e.empty());
    }

    // ---- 生命周期 ----
    {
        Yc::trees<tracked> f;
        auto a = f.emplace(f.end_tree(), 1);
        auto b = f.emplace(a.child_end(), 2);
        f.emplace(b.child_end(), 3);
        assert(tracked::alive == 3);

        f.erase(a);                              // 销毁 1、2、3
        assert(f.empty());
        assert(tracked::alive == 0);

        auto x = f.emplace(f.end_tree(), 4);
        auto y = f.emplace(x.child_end(), 5);
        assert(tracked::alive == 2);

        auto sub = f.cut(x);                     // 所有权转到 sub
        assert(f.empty());
        assert(tracked::alive == 2);

        auto moved = std::move(sub);             // move 构造，O(1)
        assert(sub.empty());
        assert(tracked::alive == 2);

        Yc::trees<tracked> dst;
        dst.emplace(dst.end_tree(), 9);
        assert(tracked::alive == 3);
        dst = std::move(moved);                  // move 赋值：先销毁 9
        assert(moved.empty());
        assert(tracked::alive == 2);
        assert(dst.begin_tree() != dst.end_tree());
        assert((*dst.begin_tree()).v == 4);
        (void)y;
    }
    assert(tracked::alive == 0);

    // ---- 范围提取的生命周期 ----
    {
        Yc::trees<tracked> g;
        auto r = g.emplace(g.end_tree(), 1);
        g.emplace(r.child_end(), 2);
        g.emplace(r.child_end(), 3);

        auto kids = g.cut_children(r);
        assert(tracked::alive == 3);
        auto all = g.cut(g.begin_tree(), g.end_tree());
        assert(g.begin_tree() == g.end_tree());
        assert(tracked::alive == 3);
    }
    assert(tracked::alive == 0);

    // ---- 拷贝构造 / 拷贝赋值 ----
    {
        Yc::trees<int> a;
        auto r = a.emplace(a.end_tree(), 1);
        a.emplace(r.child_end(), 2);
        a.emplace(r.child_end(), 3);

        Yc::trees<int> b{ a };                        // 深拷贝
        assert((collect(b.begin_tree(), b.end_tree()) == std::vector<int>{1}));
        assert((collect(b.begin_tree().child_begin(), b.begin_tree().child_end())
            == std::vector<int>{2, 3}));

        b.emplace(b.begin_tree(), 9);                 // 改副本，原树不变
        assert((collect(a.begin_tree(), a.end_tree()) == std::vector<int>{1}));
        assert((collect(a.begin_tree().child_begin(), a.begin_tree().child_end())
            == std::vector<int>{2, 3}));
        assert((collect(b.begin_tree(), b.end_tree()) == std::vector<int>{9, 1}));

        Yc::trees<int> c;
        c.emplace(c.end_tree(), 7);
        c = a;                                        // 拷贝赋值：旧内容销毁
        assert((collect(c.begin_tree(), c.end_tree()) == std::vector<int>{1}));
        assert((collect(c.begin_tree().child_begin(), c.begin_tree().child_end())
            == std::vector<int>{2, 3}));

        c = static_cast<const Yc::trees<int>&>(c);    // 自赋值
        assert((collect(c.begin_tree(), c.end_tree()) == std::vector<int>{1}));
    }

    // ---- 拷贝的生命周期 ----
    {
        Yc::trees<tracked> a;
        auto r = a.emplace(a.end_tree(), 1);
        a.emplace(r.child_end(), 2);
        assert(tracked::alive == 2);

        {
            Yc::trees<tracked> b{ a };
            assert(tracked::alive == 4);
        }
        assert(tracked::alive == 2);

        Yc::trees<tracked> c;
        c.emplace(c.end_tree(), 9);
        assert(tracked::alive == 3);
        c = a;                                        // 9 销毁，a 的 2 个复制过来
        assert(tracked::alive == 4);
    }
    assert(tracked::alive == 0);

    // ---- recur_and_write：从外部结构递归建树 ----
    {
        struct ext
        {
            int v;
            std::vector<const ext*> children;
        };
        ext e4{ 4, {} };
        ext e5{ 5, {} };
        ext e3{ 3, { &e4, &e5 } };
        ext e2{ 2, {} };
        ext e1{ 1, { &e2, &e3 } };
        ext e6{ 6, {} };

        auto vg = [](const ext* p) -> int { return p->v; };
        auto cg = [](const ext* p) -> const std::vector<const ext*>& { return p->children; };

        Yc::trees<int> t;
        std::array<const ext*, 1> hs{ &e1 };
        t.recur_and_write(t.end_tree(), vg, cg, hs);
        auto r1 = t.begin_tree();
        assert(*r1 == 1);
        assert((collect(r1.child_begin(), r1.child_end()) == std::vector<int>{2, 3}));
        auto c3 = r1.child_begin();
        ++c3;
        assert((collect(c3.child_begin(), c3.child_end()) == std::vector<int>{4, 5}));

        // 多棵树：插到 pos 前面，顺序按范围顺序
        std::array<const ext*, 2> more{ &e6, &e2 };
        t.recur_and_write(t.begin_tree(), vg, cg, more);
        assert((collect(t.begin_tree(), t.end_tree()) == std::vector<int>{6, 2, 1}));

        // 从一批根句柄构造整片森林
        std::vector<const ext*> roots{ &e1, &e6 };
        Yc::trees<int> u{ vg, cg, roots };
        assert((collect(u.begin_tree(), u.end_tree()) == std::vector<int>{1, 6}));
        auto ur = u.begin_tree();
        assert((collect(ur.child_begin(), ur.child_end()) == std::vector<int>{2, 3}));

        // pos 也可以是孩子链上的位置
        Yc::trees<int> v;
        auto vr = v.emplace(v.end_tree(), 100);
        auto vc = v.emplace(vr.child_end(), 101);
        std::array<const ext*, 1> one{ &e6 };
        v.recur_and_write(vc, vg, cg, one);
        assert((collect(vr.child_begin(), vr.child_end()) == std::vector<int>{6, 101}));
    }

    // ---- recur_and_write 的异常回滚 ----
    {
        struct ext
        {
            int v;
            std::vector<const ext*> children;
        };
        ext e2{ 2, {} };
        ext e1{ 1, { &e2 } };

        Yc::trees<int> t;
        t.emplace(t.end_tree(), 9);                   // 调用前已有的树

        auto vg = [](const ext* p) -> int
        {
            if (p->v == 2)
            {
                throw 42;                             // 在第二个节点上抛
            }
            return p->v;
        };
        auto cg = [](const ext* p) -> const std::vector<const ext*>& { return p->children; };

        bool threw = false;
        try
        {
            std::array<const ext*, 1> hs{ &e1 };
            t.recur_and_write(t.end_tree(), vg, cg, hs);
        }
        catch (int)
        {
            threw = true;
        }
        assert(threw);
        assert((collect(t.begin_tree(), t.end_tree()) == std::vector<int>{9}));
    }

    // ---- for_each：先序递归遍历 ----
    {
        Yc::trees<int> f;
        auto r = f.emplace(f.end_tree(), 1);
        auto a = f.emplace(r.child_end(), 2);
        f.emplace(r.child_end(), 3);
        f.emplace(a.child_end(), 4);
        f.emplace(f.end_tree(), 5);

        std::vector<int> order;
        f.for_each([&](int& v) { order.push_back(v); v += 10; });
        assert((order == std::vector<int>{1, 2, 4, 3, 5}));
        assert((collect(f.begin_tree(), f.end_tree()) == std::vector<int>{11, 15}));
        assert((collect(f.begin_tree().child_begin(), f.begin_tree().child_end())
            == std::vector<int>{12, 13}));

        const Yc::trees<int>& cf = f;
        int sum = 0;
        cf.for_each([&](const int& v) { sum += v; });
        assert(sum == 11 + 12 + 14 + 13 + 15);

        struct accumulator
        {
            int sum = 0;
            void operator()(const int& v) noexcept { sum += v; }
        };
        auto acc = cf.for_each(accumulator{});
        assert(acc.sum == sum);
    }

    std::puts("test_trees: ok");
    return 0;
}
