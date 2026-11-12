# Yc::trees

`Yc::trees` 是一个 header-only、分配器感知的 **N 叉树森林**容器。

- 节点由容器拥有，节点地址在搬移/排序后保持稳定；
- 每个节点内嵌兄弟链和孩子链，删除/摘出任意子树都是 O(1) 链操作；
- **没有父指针**：只支持从根向下遍历，不提供 `parent`、`depth`、`path`、LCA；
- 森林是“若干棵树”的集合，每棵树的根串在一条根链上；
- 提供 `node_proxy` 作为节点句柄，`children_view` 作为一层孩子/根链的借用视图。

完整文档见 [doc/trees_readme/trees.md](doc/trees_readme/trees.md)。

## 依赖

本库依赖侵入式双向链表：

- 仓库：[`YandereChan2/intrusive_list`](https://github.com/YandereChan2/intrusive_list)
- 头文件：[`intrusive_list.h`](https://github.com/YandereChan2/intrusive_list/blob/main/intrusive_list.h)
- 文档：[`intrusive_list_readme/intrusive_list.md`](https://github.com/YandereChan2/intrusive_list/blob/main/doc/intrusive_list_readme/intrusive_list.md)

本库**不固定依赖版本**，始终使用最新版本：

- CMake 配置时从 `main` 分支下载最新版（见 [CMakeLists.txt](CMakeLists.txt)）；
- 手动构建时 `git clone` 该仓库的最新代码即可（见下文构建测试）。

## 文件

| 文件 | 说明 |
|---|---|
| [`trees.h`](trees.h) | 森林实现 |
| [`test_trees.cpp`](test_trees.cpp) | 森林自测 |
| [`CMakeLists.txt`](CMakeLists.txt) | 构建与测试；配置时获取最新依赖 |
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

使用 CMake（配置时自动获取最新依赖，无需子模块）：

```bat
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

需要强制更新到依赖的最新版本时，删除 `build` 目录重新配置（或使用 `cmake --fresh`）。

使用 MSVC（C++23）手动构建时，先取最新依赖：

```bat
git clone https://github.com/YandereChan2/intrusive_list.git
cl /nologo /std:c++latest /EHsc /W4 /utf-8 /I . /I intrusive_list test_trees.cpp
test_trees.exe
```

预期输出：

```text
test_trees: ok
```

链表本身的测试在依赖仓库 [`YandereChan2/intrusive_list`](https://github.com/YandereChan2/intrusive_list) 中。

## 许可

MIT，见 [LICENSE](LICENSE)。
