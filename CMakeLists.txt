cmake_minimum_required(VERSION 3.20)

project(RuiZN_Server C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2 -g")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(
    ${PROJECT_SOURCE_DIR}/base
    ${PROJECT_SOURCE_DIR}/net
    ${PROJECT_SOURCE_DIR}
)

set(BASE_SOURCES
    base/log/LogStream.cc
    base/log/Logger.cc
    base/time/Timestamp.cc
)

set(NET_SOURCES
    net/poller/Poller.cc
    net/poller/epollpoller/EPollPoller.cc
    net/channel/Channel.cc
    net/eventloop/EventLoop.cc
)

add_library(base STATIC ${BASE_SOURCES})
add_library(net STATIC ${NET_SOURCES})

add_executable(test_all tests/test_all.cc)
target_link_libraries(test_all net base)

add_custom_target(run
    COMMAND test_all
    DEPENDS test_all
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
)