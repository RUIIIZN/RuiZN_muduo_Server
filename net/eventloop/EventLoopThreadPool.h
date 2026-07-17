#pragma once

#include <vector>
#include <memory>

#include "EventLoopThread.h"

namespace net
{
class EventLoopThreadPool
{
public:
    EventLoopThreadPool(EventLoop* baseLoop);
    ~EventLoopThreadPool();
    void setThreadNum(int numThreads);
    void start();
    EventLoop* getNextLoop();

private:

    int numThreads_;//loop线程数
    int next_;//下一个loop线程的索引---轮询算法

    EventLoop* baseLoop_;//使用者定义的eventloop对象
    //std::string name_;//线程池的名称
    bool started_;

    std::vector<std::unique_ptr<EventLoopThread>> threads_;//loop线程管理者
    std::vector<EventLoop*> loops_;//事件循环的逻辑执行者---保存了每个子线程中运行着的 EventLoop 对象的裸指针。
    /*
    主线程 (ThreadPool)
    │
    ├─► threads_[0] (EventLoopThread) ──创建物理线程──► 子线程 1
    │                                                   │ (栈上创建)
    │                                                   └─► loops_[0] (EventLoop)
    │
    ├─► threads_[1] (EventLoopThread) ──创建物理线程──► 子线程 2
    │                                                   │ (栈上创建)
    │                                                   └─► loops_[1] (EventLoop)


    为什么不把 EventLoop 直接写在 EventLoopThread 内部作为成员变量？

    既然每个线程都有一个 EventLoop，为什么不直接在 EventLoopThread 类里定义一个 EventLoop loop_ 成员，
    而是非要在子线程启动后动态在栈上创建、再传出指针呢？

    这是为了绝对的线程安全！

    EventLoop 对象的生命周期必须严格与其创建它的线程（即子线程自身）绑定。

    如果把 EventLoop 作为 EventLoopThread 的成员变量，由于 EventLoopThread 是在主线程中被 new 出来的，
    这会导致 EventLoop 的初始化代码在主线程中执行，从而违背了 “一个 EventLoop 只能在它自己的线程中初始化和运行” 
    的铁律，会直接引发 ownerSequence_（线程ID）检测失效或致命的并发竞态问题。

    现在的设计，通过双 vector 配合：一个管理线程实体（threads_），一个只读引用运行中的循环（loops_），
    在保证线程安全的同时，让多核 Reactor 的任务分发变得极其轻量和高效！
    */
};

}//namespace net