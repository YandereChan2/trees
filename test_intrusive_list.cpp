#include "intrusive_list.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <utility>
#include <vector>

using base = Yc::intrusive_list_node_base<void*>;
using list = Yc::basic_intrusive_list<void*>;

struct node : Yc::intrusive_list_node<void*, int>
{
    static inline int alive = 0;
    int id = 0;

    explicit node(int v, int i = 0) : id(i)
    {
        std::allocator<node> a;
        construct_value(a, v);
        ++alive;
    }

    ~node()
    {
        std::allocator<node> a;
        destroy_value(a);
        --alive;
    }
};

static const node& as_node(const base& n) noexcept
{
    return static_cast<const node&>(n);
}

static int value_of(const base& n) noexcept
{
    return as_node(n).value();
}

static auto less = [](const base& a, const base& b)
{
    return value_of(a) < value_of(b);
};

static auto even = [](const base& n)
{
    return value_of(n) % 2 == 0;
};

static auto equal = [](const base& a, const base& b)
{
    return value_of(a) == value_of(b);
};

static void delete_node(base& n)
{
    delete static_cast<node*>(&n);
}

static std::vector<int> values(const list& l)
{
    std::vector<int> out;
    for (auto it = l.begin(); it != l.end(); ++it)
    {
        out.push_back(value_of(*it));
    }
    return out;
}

static std::vector<int> ids(const list& l)
{
    std::vector<int> out;
    for (auto it = l.begin(); it != l.end(); ++it)
    {
        out.push_back(static_cast<const node&>(*it).id);
    }
    return out;
}

static void delete_remaining(list& l)
{
    std::vector<node*> remain;
    for (auto it = l.begin(); it != l.end(); ++it)
    {
        remain.push_back(static_cast<node*>(&*it));
    }
    l.clear();
    for (auto* p : remain)
    {
        delete p;
    }
}

int main()
{
    // ---- sort：稳定、按值 ----
    {
        list l;
        node* n0 = new node(3, 0);
        node* n1 = new node(1, 1);
        node* n2 = new node(2, 2);
        node* n3 = new node(2, 3);
        node* n4 = new node(3, 4);
        l.push_back(*n0);
        l.push_back(*n1);
        l.push_back(*n2);
        l.push_back(*n3);
        l.push_back(*n4);

        l.sort(less);
        assert((values(l) == std::vector<int>{1, 2, 2, 3, 3}));
        assert((ids(l) == std::vector<int>{1, 2, 3, 0, 4}));

        delete_remaining(l);
        assert(node::alive == 0);
    }

    // ---- merge：稳定 ----
    {
        list a;
        list b;
        node* a0 = new node(1, 0);
        node* a1 = new node(3, 1);
        node* a2 = new node(5, 2);
        node* b0 = new node(2, 3);
        node* b1 = new node(3, 4);
        node* b2 = new node(4, 5);
        a.push_back(*a0);
        a.push_back(*a1);
        a.push_back(*a2);
        b.push_back(*b0);
        b.push_back(*b1);
        b.push_back(*b2);

        a.merge(b, less);
        assert(b.empty());
        assert((values(a) == std::vector<int>{1, 2, 3, 3, 4, 5}));
        assert((ids(a) == std::vector<int>{0, 3, 1, 4, 5, 2}));

        // rvalue 重载
        list c;
        node* c0 = new node(0, 6);
        c.push_back(*c0);
        a.merge(std::move(c), less);
        assert(c.empty());
        assert((values(a) == std::vector<int>{0, 1, 2, 3, 3, 4, 5}));

        delete_remaining(a);
        assert(node::alive == 0);
    }

    // ---- remove_if：返回删除数量 ----
    {
        list l;
        for (int i = 1; i != 7; ++i)
        {
            l.push_back(*new node(i, i));
        }
        assert(node::alive == 6);

        auto removed = l.remove_if(even, delete_node);
        assert(removed == 3);
        assert((values(l) == std::vector<int>{1, 3, 5}));
        assert(node::alive == 3);

        delete_remaining(l);
        assert(node::alive == 0);
    }

    // ---- unique：返回删除数量 ----
    {
        list l;
        for (int v : { 1, 1, 2, 2, 2, 3 })
        {
            l.push_back(*new node(v, v));
        }
        assert(node::alive == 6);

        auto removed = l.unique(equal, delete_node);
        assert(removed == 3);
        assert((values(l) == std::vector<int>{1, 2, 3}));
        assert(node::alive == 3);

        delete_remaining(l);
        assert(node::alive == 0);
    }

    // ---- sort 比较器抛异常：节点不丢 ----
    {
        list l;
        for (int v : { 5, 1, 4, 2, 3 })
        {
            l.push_back(*new node(v, v));
        }
        assert(node::alive == 5);

        struct boom {};
        int compares = 0;
        auto bad_less = [&](const base& a, const base& b)
        {
            if (++compares == 3)
            {
                throw boom{};
            }
            return value_of(a) < value_of(b);
        };

        bool threw = false;
        try
        {
            l.sort(bad_less);
        }
        catch (boom&)
        {
            threw = true;
        }
        assert(threw);
        assert(node::alive == 5);

        auto vals = values(l);
        std::sort(vals.begin(), vals.end());
        assert((vals == std::vector<int>{1, 2, 3, 4, 5}));

        delete_remaining(l);
        assert(node::alive == 0);
    }

    // ---- 空链表 / 单节点 ----
    {
        list empty;
        empty.sort(less);
        assert(empty.empty());
        assert(empty.remove_if(even, delete_node) == 0);
        assert(empty.unique(equal, delete_node) == 0);

        list one;
        node* n = new node(1, 0);
        one.push_back(*n);
        one.sort(less);
        assert(one.begin() != one.end());
        assert(value_of(*one.begin()) == 1);
        delete_remaining(one);
        assert(node::alive == 0);
    }

    std::puts("test_intrusive_list: ok");
}
