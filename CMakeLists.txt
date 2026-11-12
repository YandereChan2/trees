cmake_minimum_required(VERSION 3.14)
project(trees LANGUAGES CXX)

enable_testing()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 依赖不固定版本：每次配置时从 main 分支取最新版本。
# 需要强制更新时删除构建目录后重新配置（或使用 cmake --fresh）。
include(FetchContent)
FetchContent_Declare(
    intrusive_list
    URL https://github.com/YandereChan2/intrusive_list/archive/refs/heads/main.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(intrusive_list)

add_library(trees INTERFACE)
target_include_directories(trees INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(trees INTERFACE intrusive_list)

add_executable(test_trees test_trees.cpp)
target_link_libraries(test_trees PRIVATE trees)
add_test(NAME test_trees COMMAND test_trees)

if(MSVC)
    target_compile_options(test_trees PRIVATE /utf-8 /W4)
else()
    target_compile_options(test_trees PRIVATE -Wall -Wextra)
endif()
