#pragma once

#include "EventLoop.h"
#include <thread>
#include <mutex>
#include <condition_variable>

namespace net
{

class EventLoop;

class EventLoopThread
{
public:
    EventLoopThread() ;
    ~EventLoopThread();
    EventLoop* startLoop();

private:
    void threadFunc();

private:
    EventLoop* loop_;
    std::mutex mutex_;
    std::condition_variable cond_;
    std::thread thread_;

};

}  // namespace net