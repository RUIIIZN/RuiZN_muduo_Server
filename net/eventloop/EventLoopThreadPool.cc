#include "EventLoopThreadPool.h"
#include "../../base/log/Logging.h"

namespace net
{
    EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseLoop) : 
    numThreads_(0), next_(0), baseLoop_(baseLoop){ LOG_INFO << "EventLoopThreadPool create" << this;}
    EventLoopThreadPool::~EventLoopThreadPool(){}

    void EventLoopThreadPool::setThreadNum(int numThreads)
    {
        LOG_INFO << "EventLoopThreadPool setThreadNum" << numThreads;
        numThreads_ = numThreads;
    }

    void EventLoopThreadPool::start()
    {
        baseLoop_->assertInLoopThread();
        LOG_DEBUG << "EventLoopThreadPool start" << this;
        started_ = true;
        //根据numThreads_创建线程的数量
        for(int i = 0; i < numThreads_; ++i)
        {
            LOG_DEBUG << "create thread start" << i;
            
            EventLoopThread* loopthread = new EventLoopThread();
            LOG_DEBUG << "EventLoopThread create" << i;

            threads_.push_back(std::unique_ptr<EventLoopThread> ((loopthread)));
            LOG_DEBUG << "push back EventLoopThread pointer" << i;

            loops_.push_back(loopthread->startLoop());
            LOG_DEBUG << "push back EventLoop pointer" << i;

            LOG_DEBUG << "create thread end" << i;
        }
        //2026.7.17.3.35 bug started_ = true;
        LOG_DEBUG << "EventLoopThreadPool start" << this;
    }

    EventLoop* EventLoopThreadPool::getNextLoop()
    {
        baseLoop_->assertInLoopThread();
        if(numThreads_ == 0) {LOG_INFO << "numThreads_ == 0";  return baseLoop_;}
        LOG_DEBUG << "EventLoopThreadPool getNextLoop " << next_;
        //轮询负载均衡策略
        EventLoop* loop = loops_[next_];
        ++next_;
        if(next_ == numThreads_) next_ = 0;

        LOG_INFO << "EventLoopThreadPool getNextLoop" << this << " " << loop;
        return loop;
    }

}