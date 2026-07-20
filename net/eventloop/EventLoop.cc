#include "EventLoop.h"
#include "../../base/log/Logging.h"

//#include "muduo/net/SocketsOps.h"
//#include "muduo/net/TimerQueue.h"

#include <algorithm>
#include <assert.h>
#include <sys/eventfd.h>
#include <sys/types.h>
#include <unistd.h>
#include "../channel/Channel.h"
#include "../poller/Poller.h"
#include "../timer/TimerQueue.h"

using namespace base;

class Poller;

namespace net
{
__thread EventLoop* t_loopInThisThread = 0;

const int kPollTimeMs = 10000;

int createEventfd()
{
  int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (evtfd < 0)
  {
    LOG_SYSERR << "Failed in eventfd";
    abort();
  }
  return evtfd;
}

void writeEventfd(int fd)
{
  uint64_t val = 1;
  ssize_t ret = ::write(fd, &val, sizeof(val));
  if(ret < 0)
  {
    LOG_SYSERR << "Failed in writeEventfd";
  }
}

void readEventfd(int fd)
{
  uint64_t val;
  ssize_t ret = ::read(fd, &val, sizeof(val));
  if(ret != sizeof(val))
  {
    LOG_SYSERR << "Failed in readEventfd";
  }
}

EventLoop* EventLoop::getEventLoopOfCurrentThread()
{
  return t_loopInThisThread;
}

EventLoop::EventLoop()
  : looping_(false),
    quit_(false),
    eventHandling_(false),
    callingPendingFunctors_(false),
    iteration_(0),
    threadId_(::gettid()),
    poller_(Poller::newDefaultPoller(this)),
    timerQueue_(new TimerQueue(this)),
    wakeupFd_(createEventfd()),
    wakeupChannel_(new Channel(this, wakeupFd_)),
    currentActiveChannel_(nullptr)
{
  wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));// 设置wakeupChannel的读事件回调函数为handleRead
  wakeupChannel_->enableReading();// 启用wakeupChannel的读事件
  LOG_DEBUG << "EventLoop " << this << " of thread " << threadId_ << " created";
}

EventLoop::~EventLoop()
{
  LOG_DEBUG << "EventLoop " << this << " of thread " << threadId_ << " destructs";
  wakeupChannel_->disableAll();//移除监控
  wakeupChannel_->remove();
  ::close(wakeupFd_);
  t_loopInThisThread = NULL;
}

void EventLoop::loop()
{
  assert(!looping_);
  assertInLoopThread();
  looping_ = true;
  quit_ = false; 
  LOG_TRACE << "EventLoop " << this << " start looping";

  while (!quit_)
  {
    //获取就绪channel进行事件处理
    activeChannels_.clear();
    pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
    ++iteration_;
    if (Logger::logLevel() <= Logger::TRACE)
    {
      printActiveChannels();
    }
    eventHandling_ = true;
    for (Channel* channel : activeChannels_)
    {
      currentActiveChannel_ = channel;
      currentActiveChannel_->handleEvent(pollReturnTime_);
    }
    currentActiveChannel_ = NULL;
    eventHandling_ = false;
    doPendingFunctors();
  }

  LOG_TRACE << "EventLoop " << this << " stop looping";
  looping_ = false;
}

void EventLoop::quit()
{
  quit_ = true;
  if (!isInLoopThread())
  {
    wakeup();
  }
}
/*
线程 A 是一个工作线程（比如主线程或专门的管理线程），
它持有 线程 B（EventLoop 线程）的指针，并在某一时刻决定关闭服务，
于是线程 A 调用了 loopB->quit()。
此时的线程 B 正在干什么？
因为当前可能没有网络事件就绪，
线程 B 正死死地阻塞在 poller_->poll()（即底层 epoll_wait）那一行代码上。
*/


void EventLoop::runInLoop(Functor cb)
{
  if (isInLoopThread())
  {
    cb();
  }
  else
  {
    queueInLoop(std::move(cb));
  }
}

void EventLoop::queueInLoop(Functor cb)
{
  {
  std::unique_lock<std::mutex> lock(mutex_);
  pendingFunctors_.push_back(std::move(cb));
  }

  if (!isInLoopThread() || callingPendingFunctors_)
  {
    wakeup();
  }
}

size_t EventLoop::queueSize()
{
  std::unique_lock<std::mutex> lock(mutex_);
  return pendingFunctors_.size();
}

TimerId EventLoop::runAt(Timestamp time, TimerCallback cb)
{
  return timerQueue_->addTimer(std::move(cb), time, 0.0);
}

TimerId EventLoop::runAfter(double delay, TimerCallback cb)
{
  Timestamp time(addTime(Timestamp::now(), delay));
  return runAt(time, std::move(cb));
}

TimerId EventLoop::runEvery(double interval, TimerCallback cb)
{
  Timestamp time(addTime(Timestamp::now(), interval));
  return timerQueue_->addTimer(std::move(cb), time, interval);
}

void EventLoop::cancelTimer(TimerId timerId)
{
  return timerQueue_->cancel(timerId);
}

void EventLoop::updateChannel(Channel* channel)//将Channel添加到poller管理中
{
  assert(channel->ownerLoop() == this);
  assertInLoopThread();
  poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel* channel)//将Channel从poller管理中移除
{
  assert(channel->ownerLoop() == this);
  assertInLoopThread();
  if (eventHandling_)
  {
    assert(currentActiveChannel_ == channel ||
    std::find(activeChannels_.begin(), activeChannels_.end(), channel) == activeChannels_.end());
/*
情况一：currentActiveChannel_ == channel
当前正在被处理事件的这个 Channel 决定自杀。例如：一个连接对应的 Socket 触发了读事件，在读回调中发现对端关闭了连接，
于是立刻在当前回调里调用 removeChannel 把自己注销掉。这是完全合法且常见的。

情况二：std::find(...) == activeChannels_.end()
当前被移除的这个 Channel，不在当前这一轮被激活的 activeChannels_ 列表中。也就是说，它今天没活干，是个闲置的 Channel，
别的活跃 Channel 在执行回调时顺便把它给移除了。这也是安全的。

如果违反了断言，意味着什么？
如果它既不是当前正在执行的 Channel，又存在于 activeChannels_ 列表中，那就会触发断言崩溃！
因为如果允许移除它，当循环后面轮到这个被移除的 Channel 执行 handleEvent 时，它可能已经被外部销毁了，
从而引发恐怖的 Use-After-Free（野指针内存崩溃）
*/
  }
  poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel* channel)//判断Channel是否在poller管理中
{
  assert(channel->ownerLoop() == this);
  assertInLoopThread();
  return poller_->hasChannel(channel);
}

// void EventLoop::abortNotInLoopThread()
// {
//   LOG_FATAL << "EventLoop::abortNotInLoopThread - EventLoop " << this
//             << " was created in threadId_ = " << threadId_
//             << ", current thread id = " <<  CurrentThread::tid();
// }

void EventLoop::wakeup()
{
    writeEventfd(wakeupFd_);
}

void EventLoop::handleRead()
{
    readEventfd(wakeupFd_);
}

void EventLoop::assertInLoopThread()
{
    LOG_DEBUG << "Loop threadId_: " << threadId_ << ", Current thread tid: " << ::gettid();
    assert(isInLoopThread());
    LOG_DEBUG << "EventLoop assertInLoopThread" << this;
}

void EventLoop::doPendingFunctors()
{
  std::vector<Functor> functors;
  callingPendingFunctors_ = true;

  {
  std::unique_lock<std::mutex> lock(mutex_);
  functors.swap(pendingFunctors_);
  }

  for (const Functor& functor : functors)
  {
    functor();
  }
  callingPendingFunctors_ = false;
}

void EventLoop::printActiveChannels() const
{
  for (const Channel* channel : activeChannels_)
  {
    LOG_TRACE << "{" << channel->reventsToString() << "} ";
  }
}

}  // namespace net