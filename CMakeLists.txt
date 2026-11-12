cmake_minimum_required(VERSION 3.14)
project(trees LANGUAGES CXX)

enable_testing()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(YC_INTRUSIVE_LIST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/intrusive_list)
if(NOT EXISTS ${YC_INTRUSIVE_LIST_DIR}/intrusive_list.h)
    message(FATAL_ERROR
        "缺少依赖 intrusive_list.h：请先运行 git submodule update --init")
endif()

add_library(trees INTERFACE)
target_include_directories(trees INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${YC_INTRUSIVE_LIST_DIR})

add_executable(test_trees test_trees.cpp)
target_link_libraries(test_trees PRIVATE trees)
add_test(NAME test_trees COMMAND test_trees)

if(MSVC)
    target_compile_options(test_trees PRIVATE /utf-8 /W4)
else()
    target_compile_options(test_trees PRIVATE -Wall -Wextra)
endif()
