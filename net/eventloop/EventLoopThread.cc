#include "EventLoopThread.h"
#include <mutex>

namespace net
{
EventLoopThread::EventLoopThread() : loop_(NULL) ,thread_(){}//超级bug：忘了初始化thread

EventLoopThread::~EventLoopThread() 
{
    loop_->quit();
    thread_.join();
}

EventLoop* EventLoopThread::startLoop() 
{
    thread_ = std::thread([this]() { this->threadFunc(); });
    LOG_DEBUG << "threadfunc set finish";
    EventLoop* loop = NULL;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this]{return loop_ != nullptr;});
        //获取loop指针必须要确保loop_已赋值完毕
        loop = loop_;
    }  
    return loop;
}

void EventLoopThread::threadFunc()
{
    EventLoop loop;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_all();//唤醒所有的阻塞等待
    }
    loop.loop();
    loop_ = nullptr;
}

}//namespace net