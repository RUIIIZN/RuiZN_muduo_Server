cmake_minimum_required(VERSION 3.10)
project(RuiZN_Server CXX)

# 支持 C++17 及以上标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -------------------------------------------------------------------
# 1. 头文件包含路径 (Include Directories)
# -------------------------------------------------------------------
# 这样你在代码里就可以直接写 #include "base/time/Timestamp.h" 或 "net/timer/Timer.h"
include_directories(
    ${PROJECT_SOURCE_DIR}
)

# -------------------------------------------------------------------
# 2. 收集 base 模块的源文件
# -------------------------------------------------------------------
file(GLOB_RECURSE BASE_SOURCES 
    "base/*.cc"
    "base/*.cpp"
)

# -------------------------------------------------------------------
# 3. 收集 net 模块的源文件
# -------------------------------------------------------------------
file(GLOB_RECURSE NET_SOURCES 
    "net/*.cc"
    "net/*.cpp"
)

# -------------------------------------------------------------------
# 4. 生成静态库
# -------------------------------------------------------------------
# 编译基础库 base
add_library(base STATIC ${BASE_SOURCES})

# 编译网络库 net
add_library(net STATIC ${NET_SOURCES})

# -------------------------------------------------------------------
# 5. 编译测试程序
# -------------------------------------------------------------------
# 根据你截图的目录结构，test.cc 位于 examples/test.cc 
# (如果实际在 tests/test.cc，请将路径改为 tests/test.cc)
add_executable(test examples/test.cc)

# 链接静态库。注意：net 依赖 base，所以 net 写在前面，base 写在后面
target_link_libraries(test net base pthread)

# -------------------------------------------------------------------
# 6. 一键运行命令：make run
# -------------------------------------------------------------------
add_custom_target(run
    COMMAND ./test
    DEPENDS test
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
)