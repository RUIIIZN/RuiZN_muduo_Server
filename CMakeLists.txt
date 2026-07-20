cmake_minimum_required(VERSION 3.10)
project(RuiZN_Server CXX)

# 支持 C++17 及以上标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -------------------------------------------------------------------
# 1. 头文件包含路径 (Include Directories)
# -------------------------------------------------------------------
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
# 5. 编译服务端与客户端可执行程序
# -------------------------------------------------------------------
# 5.1 编译服务端程序 ser
add_executable(ser examples/ser.cc)
target_link_libraries(ser net base pthread)

# 5.2 编译客户端程序 cli
add_executable(cli examples/cli.cc)
target_link_libraries(cli net base pthread)

# -------------------------------------------------------------------
# 6. 一键运行命令：make run_ser / make run_cli
# -------------------------------------------------------------------
# 6.1 一键运行服务端
add_custom_target(run_ser
    COMMAND ./ser
    DEPENDS ser
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
)

# 6.2 一键运行客户端
add_custom_target(run_cli
    COMMAND ./cli
    DEPENDS cli
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
)