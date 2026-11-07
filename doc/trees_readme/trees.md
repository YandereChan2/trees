# Yc::trees（森林）

实现位于 [trees.h](../../trees.h)，测试位于 [test_trees.cpp](../../test_trees.cpp)。

在头文件 `"trees.h"` 中定义：

```cpp
template<class T, class Alloc = std::allocator<T>>
class trees;
```

`Yc::trees` 是一个**拥有节点**的 N 叉树森林：

- 一片森林由若干棵树组成，每棵树的根节点串在一条根链上；
- 每个节点内部带一条孩子链，所以一个节点可以有任意多个孩子；
- 没有父指针，只能从根向下遍历；
- 未被删除的节点地址在插入、删除、搬移、排序之后都保持不变；
- 节点内存由 `Alloc` 重新绑定后的节点分配器管理。

它适合“动态树的基本操作”：建节点、删子树、摘/接子树、重排兄弟、遍历、复制、交换、按值排序孩子。它**不是** tree.hh 那样的万能树库：没有 `parent`、`depth`、`index`、`path`、LCA、`size`、`find`、`flatten` 等接口。

---

## 1. 快速开始

```cpp
#include "trees.h"
#include <cassert>

int main()
{
    Yc::trees<int> f;

    // 在森林末尾加一棵树
    auto root = f.emplace(f.end_tree(), 1);

    // 给 root 追加孩子
    auto a = f.emplace(root.child_end(), 2);
    auto b = f.emplace(root.child_end(), 3);

    // 给 a 加孩子
    f.emplace(a.child_end(), 21);

    // 根链：1
    assert(*f.begin_tree() == 1);
    assert(f.begin_tree().child_begin() != f.begin_tree().child_end());

    // root 的直接孩子：2、3
    int sum = 0;
    for (auto p : root.all_children())
    {
        sum += *p;
    }
    assert(sum == 5);

    // 整片森林先序：1、2、21、3
    sum = 0;
    f.for_each([&](int& v) { sum += v; });
    assert(sum == 27);
}
```

`emplace` / `insert` 的语义是**插在 `pos` 左边（前面）**：

```cpp
f.emplace(f.end_tree(), v);            // 森林末尾追加一棵树
f.emplace(parent.child_end(), v);      // parent 的孩子末尾追加
f.emplace(pos, v);                     // 在 pos 左边插入一个兄弟
f.insert(pos, v);                      // 同上，值版本
```

要插在某个节点**后面**，可以传它的后继：

```cpp
auto after = pos;
++after;
f.emplace(after, v);                   // pos 是最后一个时，after 是 end，等价于追加
```

---

## 2. 接口总览

| 类别 | 接口 |
|---|---|
| 构造/赋值 | `trees()`、`trees(alloc)`、拷贝/移动构造、拷贝/移动赋值、`trees(vg, cg, roots, alloc={})`、析构 |
| 观察 | `empty()`、`get_allocator()` |
| 根/孩子范围 | `begin_tree()` / `end_tree()`、`all_children()`、`sort_children()` |
| 建节点 | `emplace(pos, args...)`、`insert(pos, value)` |
| 删除 | `erase(p)`、`erase(first, last)`、`clear()` |
| 摘出 | `cut(p)`、`cut(first, last)`、`cut_children(parent)` |
| 装回/搬移 | `splice(pos, trees&&)`、`splice(pos, node)`、`splice(pos, first, last)` |
| 合并 | `merge(trees&&)` |
| 交换 | `static swap_sub_tree(l, r)`、`static swap_node(l, r)` |
| 递归构建 | `recur_and_write(pos, vg, cg, handles)` |
| 整树遍历 | `for_each(f)`（可变/const） |
| 代理 | `node_proxy` / `node_const_proxy`：`*`、`->`、`++/--`、`==`、`child_begin/end`、`all_children`、`sort_children` |
| 子视图 | `children_view` / `children_const_view` / `children_iterator` / `children_const_iterator` |

---

## 3. 结构模型

```text
trees
 └─ roots: [R1] ⇄ [R2] ⇄ ...        // 根链（循环链表，带内嵌哨兵）
       │
       └─ R1.children: [C1] ⇄ [C2] ⇄ ...
                          │
                          └─ C1.children: ...
```

- `roots` 是一条 `basic_intrusive_list`，元素是根节点；
- 每个 `trees_node` 内含一条 `children` 链；
- 每个链表有一个内嵌哨兵，空链的 `begin() == end()`；
- 节点本身不持有父指针，只持有兄弟链的 `prev/next` 和孩子链哨兵；
- 每个节点大约多出 4 个指针的开销（兄弟前后 + 孩子哨兵前后），外加 `T`；
- 不存储 `size`，`size()` 需要自己遍历统计。

节点地址稳定：`emplace` 之后，除非 `erase`/`clear` 销毁该节点，否则代理、引用、指针一直有效；`splice`/`sort` 只改链接，不搬节点。

---

## 4. 模板参数与成员类型

| 名称 | 说明 |
|---|---|
| `T` | 元素类型。需要能按 `Alloc` 构造/析构。 |
| `Alloc` | 元素分配器；内部会 rebind 成节点分配器。默认 `std::allocator<T>`。 |

| 成员类型 | 定义 |
|---|---|
| `value_type` | `T` |
| `allocator_type` | `Alloc` |
| `pointer` | `std::allocator_traits<Alloc>::pointer` |
| `const_pointer` | `std::allocator_traits<Alloc>::const_pointer` |
| `void_pointer` | `std::allocator_traits<Alloc>::void_pointer` |
| `const_void_pointer` | `std::allocator_traits<Alloc>::const_void_pointer` |
| `reference` | `T&` |
| `const_reference` | `const T&` |
| `node_proxy` | 可变节点代理（同时是兄弟链上的双向迭代器） |
| `node_const_proxy` | 常量节点代理 |
| `children_view` | `children_view<node_proxy>`，根/孩子链的借用视图 |
| `children_const_view` | `children_view<node_const_proxy>` |
| `children_iterator` | `children_view` 的可变迭代器 |
| `children_const_iterator` | `children_view` 的常量迭代器 |

`pointer` / `const_pointer` / `void_pointer` / `const_void_pointer` 全部来自 `std::allocator_traits<Alloc>`，没有硬编码为 `T*`，因此 fancy pointer 分配器也能得到一致的别名。

---

## 5. 构造、赋值、析构

```cpp
trees() noexcept;
explicit trees(const Alloc& a) noexcept;

trees(const trees& other);
trees& operator=(const trees& other);

template<class ValueGetter, class ChildrenGetter, class RangeOfHandles>
trees(ValueGetter vg, ChildrenGetter cg, RangeOfHandles roots, const Alloc& a = {});

trees(trees&& other) noexcept(/*...*/);
trees& operator=(trees&& other) noexcept(/*...*/);
~trees();
```

| 接口 | 说明 |
|---|---|
| 默认构造 | 空森林 |
| `trees(alloc)` | 空森林，指定分配器 |
| 拷贝构造 | 深拷贝整片森林；分配器按 `select_on_container_copy_construction` 选择 |
| 拷贝赋值 | 先在临时森林里完整复制，复制成功后再替换；复制失败时目标不变；遵循 `propagate_on_container_copy_assignment` |
| 范围构造 | 从外部树形结构的根句柄范围递归建树，见第 11 节 |
| 移动构造/赋值 | O(1) 整链搬运，源森林变空；移动赋值会先清空自己 |
| 析构 | 销毁所有节点 |

整片森林也可以用 `std::swap(a, b)` 交换（通过移动构造/赋值实现，O(1)）。

---

## 6. 观察与遍历入口

```cpp
bool empty() const noexcept;
Alloc get_allocator() const noexcept;

node_proxy begin_tree() noexcept;
node_proxy end_tree() noexcept;
node_const_proxy begin_tree() const noexcept;
node_const_proxy end_tree() const noexcept;

children_view all_children() noexcept;
children_const_view all_children() const noexcept;

template<class Compare>
void sort_children(Compare comp);
```

- `empty()`：根链是否为空，O(1)；
- `get_allocator()`：返回分配器副本；
- `begin_tree()/end_tree()`：根链的起止代理，空森林时相等；
- `all_children()`：**只遍历一层**的借用视图：
  - 对 `trees` 是根链（所有树的根）；
  - 对 `node_proxy` 是该节点的直接孩子；
  - 元素是 `node_proxy` 右值，可以直接 `*p` 或 `p.child_begin()`；
- `sort_children(comp)`：对**一层**按值稳定排序：
  - `trees::sort_children` 排根链；
  - `node_proxy::sort_children` 排该节点的直接孩子；
  - 比较器接收 `const T&`；
  - 孙辈不参与；
  - `node_const_proxy` 上不可用。

```cpp
f.sort_children(std::less<>{});
root.sort_children([](const int& a, const int& b) { return a > b; });
```

---

## 7. 节点代理 `node_proxy`

`node_proxy` 既是“兄弟链上的双向迭代器”，也是“节点句柄”。

| 操作 | 说明 |
|---|---|
| `*p` | 返回节点携带的 `T&`（`node_const_proxy` 返回 `const T&`） |
| `p->member` | 访问 `T` 的成员 |
| `++p` / `--p` | 在同一条兄弟链上前移/后移（不是进入孩子） |
| `p == q` | 比较底层迭代器 |
| `p.child_begin()` | 第一个孩子 |
| `p.child_end()` | 孩子链末尾哨兵 |
| `p.all_children()` | 直接孩子的借用视图 |
| `p.sort_children(comp)` | 对直接孩子稳定排序（仅可变代理） |
| `p.base()` | 底层链表迭代器，实现细节，不建议用户依赖 |

注意：

- 默认构造的代理无效；不要解引用；
- 指向 `end_tree()` / `child_end()` 的代理是合法的“位置”，但**不可解引用**；
- 不能从单个代理判断它是不是 end；
- 没有 `parent()`、`depth()`、`index()`、`path()`；
- `node_proxy` 可以隐式转换为 `node_const_proxy`；
- 代理的 `const` 不等于元素的 `const`：`const node_proxy` 仍然可以改它指向的 `T`；要只读请用 `node_const_proxy`。

---

## 8. 借用视图 `children_view`

`children_view` 是 `Yc::details` 里的实现，通过 `trees` 的别名公开：

```cpp
template<class Proxy>
class children_view;

// 对 trees<T>：
using children_view = details::children_view<node_proxy>;
using children_const_view = details::children_view<node_const_proxy>;
```

### 构造

```cpp
children_view(Proxy first, Proxy last) noexcept;
explicit children_view(std::pair<Proxy, Proxy> ends) noexcept;
```

例如：

```cpp
Yc::trees<int>::children_view v{ r.child_begin(), r.child_end() };

Yc::trees<int>::children_view v2{
    std::pair{ r.child_begin(), r.child_end() } };
```

### 特性

- 只借用 `[first, last)`，不拥有节点；
- `begin()`/`end()` 返回 `children_iterator`；
- 迭代器 `operator*` **按值返回 `node_proxy`**（右值），不是 `T&`；
- 双向迭代器；
- 满足 `std::ranges::view`、`bidirectional_range`、`borrowed_range`；
- 构造和 `begin/end` 都是 O(1)，不分配内存；
- 节点被销毁会使其代理/迭代器失效；遍历期间不要增删或重排孩子链。

```cpp
for (auto p : f.all_children())      // p 是 node_proxy
{
    *p += 1;
}
```

---

## 9. 插入与删除

### 插入

```cpp
template<class... Args>
node_proxy emplace(node_proxy pos, Args&&... args);

node_proxy insert(node_proxy pos, const T& value);
node_proxy insert(node_proxy pos, T&& value);
```

- 在 `pos` 左侧插入一个新节点；
- 返回新节点代理；
- 一次节点分配 + 一次 `T` 构造；
- 如果分配或 `T` 构造抛异常，森林不变（RAII 守卫负责回收）；
- `pos` 可以是任意兄弟链上的位置，也可以是该链的 `end()`。

### 删除

```cpp
void erase(node_proxy p) noexcept;
void erase(node_proxy first, node_proxy last) noexcept;
```

- `erase(p)` 删除 `p` 及其**整棵子树**，O(子树大小)；
- `erase(first, last)` 删除同一兄弟链上 `[first, last)` 的每棵子树，`last` 可以是 `end_tree()`/`child_end()`；
- 删除时先父后子，额外空间 O(1)；
- `erase(p)` 不需要父指针：节点自己的 `prev/next` 足够把它从任意链上摘下来；
- 假设 `T` 的析构不抛异常。

如果只想删节点、保留孩子：

```cpp
auto kids = f.cut_children(p);       // p 的孩子变成一片森林
auto after = p;
++after;                             // p 的后继位置
f.erase(p);                          // 删掉 p（此时 p 已经没有孩子）
f.splice(after, std::move(kids));    // 把孩子们插回原位置
```

---

## 10. 摘出、装回、合并

### 摘出

```cpp
trees cut(node_proxy p) noexcept;
trees cut(node_proxy first, node_proxy last) noexcept;
trees cut_children(node_proxy parent) noexcept;
```

- `cut(p)`：把 `p` 这棵子树摘下来，成为新森林里的一棵树；
- `cut(first, last)`：把同一兄弟链上 `[first, last)` 摘成新森林；`first == last` 得到空森林；
- `cut_children(parent)`：把 `parent` 的全部孩子摘成新森林，`parent` 变叶子；
- 结构操作 O(1)；
- 返回的森林使用当前分配器的一个副本；
- 不销毁任何节点。

### 装回

```cpp
void splice(node_proxy pos, trees&& sub) noexcept;
void splice(node_proxy pos, node_proxy node) noexcept;
void splice(node_proxy pos, node_proxy first, node_proxy last) noexcept;
```

- 把整片森林 / 单个节点 / 一段兄弟插到 `pos` 之前；
- 源变空（整片森林）或源链失去该节点/区间；
- O(1)；
- `splice(pos, sub)` 要求 `pos` 不在 `sub` 里；
- `splice(pos, node)` 要求 `pos != node`，且 `pos` 不在 `node` 的子树里；
- `splice(pos, first, last)` 要求 `first/last` 同链、`pos` 不在区间里；
- 跨森林搬运要求两边的分配器能互相释放（通常就是相等）；库不检查。

### 合并

```cpp
void merge(trees&& other) noexcept;
```

把 `other` 的整条根链追加到本森林末尾，`other` 变空，O(1)。不排序；需要排序用 `sort_children`。

### 移动到某个节点后面

```cpp
auto after = pos;
++after;
f.splice(after, node);
```

`pos` 是最后一个时，`after` 是 end，`splice` 等价于追加。

---

## 11. 递归构造与复制

```cpp
template<class ValueGetter, class ChildrenGetter, class RangeOfHandles>
trees(ValueGetter vg, ChildrenGetter cg, RangeOfHandles roots,
      const Alloc& a = {});

template<class ValueGetter, class ChildrenGetter, class RangeOfHandles>
void recur_and_write(node_proxy pos, ValueGetter vg, ChildrenGetter cg,
                     RangeOfHandles handles);
```

- `vg(h)`：从句柄 `h` 取当前节点的值；
- `cg(h)`：返回 `h` 的孩子句柄范围；
- 每个句柄都必须有效，本接口**不做判空**；
- `recur_and_write` 在临时森林里构造，成功后整片插到 `pos` 之前；抛异常时目标森林不变；
- 根范围可以是任意 range，包括 `f.all_children()`。

### 复制整片森林

```cpp
auto vg = [](Yc::trees<int>::node_proxy p) -> int { return *p; };
auto cg = [](Yc::trees<int>::node_proxy p) { return p.all_children(); };

Yc::trees<int> copy{ vg, cg, f.all_children() };
```

这里 `f.all_children()` 是根代理的范围，`cg` 返回每一层的直接孩子代理范围。

### 复制一棵子树

```cpp
auto src = f.begin_tree();                     // 要复制的子树根
std::array<Yc::trees<int>::node_proxy, 1> roots{ src };

Yc::trees<int> dst;
dst.recur_and_write(dst.end_tree(), vg, cg, roots);
```

`recur_and_write` 插入的是“整片森林”；只有一个根时就是复制一棵子树。要挂到某个父节点下，把 `pos` 换成 `parent.child_end()`。

### 从外部结构建树

```cpp
struct ext
{
    int v;
    std::vector<const ext*> children;
};

auto vg = [](const ext* p) -> int { return p->v; };
auto cg = [](const ext* p) -> const std::vector<const ext*>& { return p->children; };

std::vector<const ext*> roots{ &e1, &e6 };
Yc::trees<int> t{ vg, cg, roots };
```

句柄可以是 `node_proxy`、指针、`std::reference_wrapper` 等；只要 `vg`/`cg` 能接受它，并且范围里的元素都有效。

---

## 12. 遍历

### 根

```cpp
for (auto it = f.begin_tree(); it != f.end_tree(); ++it)
{
    // *it 是根的 T
}

for (auto p : f.all_children())   // p 是根代理
{
    // *p 是根的 T
}
```

### 直接孩子

```cpp
for (auto c = p.child_begin(); c != p.child_end(); ++c)
{
    // *c 是孩子的 T
}

for (auto c : p.all_children())   // c 是孩子代理
{
    // *c
}
```

### 整片森林

```cpp
f.for_each([](int& v) { /* v 是 T& */ });
```

`for_each`：

- 先序：父节点先于孩子，兄弟按链顺序；
- 有两种重载：`T&` 和 `const T&`；
- 与 `std::for_each` 一样，按值传入、按值返回函数对象，有状态的对象可以把状态带出来；
- 不提供提前退出；不要在 `f` 内增删/摘接节点，但可以改元素值；
- 只覆盖整片森林；要遍历单棵子树，用 `p.all_children()` 自己递归。

```cpp
void visit(Yc::trees<int>::node_proxy p)
{
    use(*p);
    for (auto c : p.all_children())
    {
        visit(c);
    }
}
```

---

## 13. 交换

```cpp
static void swap_sub_tree(node_proxy l, node_proxy r) noexcept;
static void swap_node(node_proxy l, node_proxy r) noexcept;
```

两个函数都是 `static`，不需要分配器，O(1)，只调整指针，不移动 `T`。

### `swap_sub_tree`

交换两个节点在兄弟链上的**位置**，连同各自的孩子：

- 适合“两棵子树换位置”；
- 要求 `l != r`，且两者**不互为祖先**；
- 相邻、非相邻、同链、跨链走同一条路径；
- 允许跨森林，但两边分配器必须能互相释放。

### `swap_node`

交换两个节点的位置，并交换它们的两个子链，净效果是：

- 两个位置上的**节点身份/值**互换；
- **孩子留在原位**；
- 树形不变。

```text
交换前：          交换后：
  R(1)              R(2)
  ├─ A(2)           ├─ A(1)
  │  └─ 21          │  └─ 21
  └─ B(3)           └─ B(3)
```

`swap_node(R, A)` 之后根位置是 2，孩子位置是 1；21 仍然挂在“孩子位置”下面。

- 要求 `l != r`；
- 支持父子、祖先/后代、同链、跨链、相邻；
- O(1)，不移动 `T`；
- 允许跨森林，但两边分配器必须能互相释放。

---

## 14. 排序孩子

```cpp
template<class Compare>
void trees<T>::sort_children(Compare comp);

template<class Compare> requires (!Const)
void node_proxy::sort_children(Compare comp) const;
```

- 只排一层：`trees` 排根链，`node_proxy` 排直接孩子；
- 比较器接收 `const T&`；
- 稳定排序；
- O(k log k) 次比较，O(1) 额外空间（固定 64 个 bin），不分配；
- 节点只是重连，代理/引用仍有效；
- 比较器抛异常时，孩子链仍完整，顺序不保证；
- `node_const_proxy` 不可用。

```cpp
f.sort_children(std::less<>{});
root.sort_children([](const int& a, const int& b) { return a > b; });
```

---

## 15. 清空

```cpp
void clear() noexcept;
```

销毁森林里的所有树，O(n)。析构函数会调用它。

---

## 16. 复杂度

| 操作 | 复杂度 |
|---|---|
| `empty` | O(1) |
| `begin_tree/end_tree` | O(1) |
| `all_children().begin()/end()` | O(1) |
| `emplace` / `insert` | O(1) + 一次分配 + `T` 构造 |
| `erase(p)` | O(p 的子树大小) |
| `erase(first, last)` | O(区间内节点总数) |
| `cut` / `cut_children` | O(1) |
| `splice` | O(1) |
| `merge` | O(1) |
| `swap_sub_tree` / `swap_node` | O(1) |
| `sort_children` | O(k log k)，O(1) 额外空间 |
| `recur_and_write` / 范围构造 | O(新增节点数) |
| `for_each` | O(n) |
| 拷贝构造/赋值 | O(n) |
| `clear` / 析构 | O(n) |
| `size` | 不提供；需要 O(n) 自己统计 |

---

## 17. 前置条件与注意事项

库为了保持侵入式和 O(1)，不做运行时检查。以下条件由调用方保证：

| 操作 | 前置条件 |
|---|---|
| `emplace(pos, ...)` / `insert` | `pos` 是有效位置（某条兄弟链上的节点或该链的 end） |
| `erase(p)` | `p` 是有效节点，不是 end |
| `erase(first, last)` | 同一兄弟链，`last` 可由 `first` 到达 |
| `cut(first, last)` | 同一兄弟链，`first` 在 `last` 之前 |
| `splice(pos, sub)` | `pos` 不在 `sub` 里；跨森林分配器可互相释放 |
| `splice(pos, node)` | `pos != node`；`pos` 不在 `node` 的子树里 |
| `splice(pos, first, last)` | 同一兄弟链；`pos` 不在 `[first, last)` 里 |
| `merge(other)` | 两森林分配器可互相释放 |
| `swap_sub_tree(l, r)` | `l != r`，两者不互为祖先 |
| `swap_node(l, r)` | `l != r` |
| `sort_children(comp)` | `comp` 是 `const T&` 上的严格弱序 |
| `recur_and_write` / 范围构造 | 所有句柄有效；`cg` 返回有效孩子范围 |
| `for_each` | 不要在回调里增删/摘接节点 |

其他注意点：

- `end_tree()` / `child_end()` 是哨兵，不可解引用；
- 不能从单个 `node_proxy` 判断它是不是 end；循环必须成对持有 `begin/end`；
- 没有父指针：`erase(p)`/`cut(p)` 不需要父节点，但 `parent(p)`、`depth(p)`、`index(p)`、`path`、LCA 不支持；
- 整片森林没有 `size()`；需要的话自己用 `for_each` 统计，或在外部维护计数；
- 没有整树迭代器；整片森林用 `for_each`，单子树用 `all_children` 递归；
- 跨森林操作（`splice`、`merge`、`swap_*`）要求分配器能互相释放；否则行为未定义；
- 结构操作会改变兄弟链顺序，但不会让其他节点的代理失效；被删除节点的代理/引用会失效；
- 不是线程安全的，需要外部同步。

---

## 18. 异常安全

- `emplace` / `insert`：强异常安全；分配或 `T` 构造失败不会留下半截节点；
- 拷贝构造：强异常安全；
- 拷贝赋值：复制阶段失败时目标不变；替换阶段取决于分配器移动赋值是否抛异常；
- 范围构造：失败时清掉已建的部分；
- `recur_and_write`：先建临时森林，失败时目标森林不变；
- `erase` / `clear` / 析构：`noexcept`，假设 `T` 的析构不抛；
- `cut` / `splice` / `merge` / `swap_*`：`noexcept`，只改指针；
- `sort_children`：比较器抛异常时孩子链仍完整（顺序不保证），不会丢节点；
- `for_each`：回调抛异常会向外传播；结构不变（前提是回调没有改结构）。

---

## 19. 设计取舍

这个库只覆盖“无父指针的动态树基本操作”：

- **有**：建、删、摘、接、合并、交换、一层排序、先序遍历、深拷贝、递归构造；
- **没有**：父指针、深度、下标、路径、LCA、O(1) 大小、查找、flatten、wrap、层序迭代器、子树迭代器。

没有父指针的代价是向上查询必须由调用方自己带路径栈；好处是：

- 删除/摘出任意节点只需要节点代理，不需要先找父节点；
- 节点地址稳定，代理和引用不会因为兄弟重排失效；
- `cut`/`splice`/`merge`/`swap` 都是 O(1) 指针操作。

如果业务需要深度或路径，建议在进入子树时携带一条 `std::vector<node_proxy>` 路径栈，而不是给每个节点加父指针。

---

## 20. 构建与测试

仓库中的自测使用 MSVC：

```bat
cl /nologo /std:c++latest /EHsc /W4 /utf-8 /I C:\code\Yc ^
   C:\code\Yc\test_trees.cpp

test_trees.exe
```

链表层的排序/删除测试：

```bat
cl /nologo /std:c++latest /EHsc /W4 /utf-8 /I C:\code\Yc ^
   C:\code\Yc\test_intrusive_list.cpp

test_intrusive_list.exe
```

两个测试当前的输出分别是 `test_trees: ok` 和 `test_intrusive_list: ok`。
