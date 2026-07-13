cmake_minimum_required(VERSION 3.10)

project(RuiZN_Server)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")

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
)

add_library(base STATIC ${BASE_SOURCES})
add_library(net STATIC ${NET_SOURCES})

add_executable(test_server examples/main.cc)
target_link_libraries(test_server net base)

add_custom_target(run
    COMMAND test_server
    DEPENDS test_server
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)