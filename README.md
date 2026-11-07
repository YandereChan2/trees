# Yc::trees

`Yc::trees` 是一个 head-only、分配器感知的 **N 叉树森林**容器。

- 节点由容器拥有，节点地址在搬移/排序后保持稳定；
- 每个节点内嵌兄弟链和孩子链，删除/摘出任意子树都是 O(1) 链操作；
- **没有父指针**：只支持从根向下遍历，不提供 `parent`、`depth`、`path`、LCA；
- 森林是“若干棵树”的集合，每棵树的根串在一条根链上；
- 提供 `node_proxy` 作为节点句柄，`children_view` 作为一层孩子/根链的借用视图。

完整文档见 [doc/trees_readme/trees.md](doc/trees_readme/trees.md)。

## 文件

| 文件 | 说明 |
|---|---|
| [`trees.h`](trees.h) | 森林实现 |
| [`intrusive_list.h`](intrusive_list.h) | 依赖：侵入式双向链表 |
| [`test_trees.cpp`](test_trees.cpp) | 森林自测 |
| [`test_intrusive_list.cpp`](test_intrusive_list.cpp) | 链表层排序/删除自测 |
| [`doc/trees_readme/trees.md`](doc/trees_readme/trees.md) | 使用文档 |

## 快速开始

```cpp
#include "trees.h"
#include <cassert>

int main()
{
    Yc::trees<int> f;

    auto root = f.emplace(f.end_tree(), 1);      // 加一棵树
    auto a = f.emplace(root.child_end(), 2);     // 给 root 加孩子
    auto b = f.emplace(root.child_end(), 3);     // 再加一个
    f.emplace(a.child_end(), 21);                // 给 a 加孩子

    int sum = 0;
    for (auto p : root.all_children())           // 只遍历直接孩子
    {
        sum += *p;
    }
    assert(sum == 5);

    sum = 0;
    f.for_each([&](int& v) { sum += v; });       // 整片森林先序
    assert(sum == 27);

    f.sort_children([](const int& x, const int& y) { return x < y; });
}
```

## 构建测试

使用 MSVC（C++23）：

```bat
cl /nologo /std:c++latest /EHsc /W4 /utf-8 /I . test_trees.cpp
test_trees.exe

cl /nologo /std:c++latest /EHsc /W4 /utf-8 /I . test_intrusive_list.cpp
test_intrusive_list.exe
```

预期输出：

```text
test_trees: ok
test_intrusive_list: ok
```

## 许可

MIT，见 [LICENSE](LICENSE)。
