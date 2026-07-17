#pragma once

#include <atomic>
#include <cstdlib>
#include <functional>
#include <thread>
#include <vector>
#include <memory>
#include <any>
#include <mutex>
#include <unistd.h>


#include "../../base/time/Timestamp.h"
#include "../../base/log/Logging.h"
#include "../../net/timer/TimerId.h"
#include "../timer/Timer.h"
//#include "../../muduo/base/CurrentThread.h"

using base::Timestamp;
namespace net
{

class Channel;
class Poller;
class TimerQueue;

class EventLoop 
{
 public:
  typedef std::function<void()> Functor;

  EventLoop();
  ~EventLoop();  

  void loop();//开始事件循环，处理事件和任务
  void quit();//退出事件循环，如果当前处于事件监控中则唤醒循环，退出循环

  Timestamp pollReturnTime() const { return pollReturnTime_; }
  int64_t iteration() const { return iteration_; }

  void runInLoop(Functor cb);
  void queueInLoop(Functor cb);

  size_t queueSize();

  //定时器相关
  TimerId runAt(Timestamp time, TimerCallback cb);//在指定时间执行定时任务
  TimerId runAfter(double delay, TimerCallback cb);//在指定延迟时间执行定时任务
  TimerId runEvery(double interval, TimerCallback cb);//在指定间隔时间执行定时任务
  void cancelTimer(TimerId timerId);//取消定时任务


  void wakeup();
  void updateChannel(Channel* channel);//通过poller添加监控
  void removeChannel(Channel* channel);
  bool hasChannel(Channel* channel);//判断channel是否添加过监控管理

  void assertInLoopThread();//断言当前是否处于 事件循环线程中

  bool isInLoopThread() const { return threadId_ == ::gettid();}//是否在事件循环线程中 !!!!      2026.7.17应该是gettid写成了getpid，笑死
  bool callingPendingFunctors() const { return callingPendingFunctors_; }//是否正在处理任务池中的任务
  bool eventHandling() const { return eventHandling_; }//是否正在处理事件中---poller中的

  void setContext(const std::any& context) { context_ = context; }

  const std::any& getContext() const
  { return context_; }

  std::any* getMutableContext()
  { return &context_; }

  static EventLoop* getEventLoopOfCurrentThread();

 private:
  void abortNotInLoopThread();
  void handleRead();  // waked up
  void doPendingFunctors();

  void printActiveChannels() const; // DEBUG

  typedef std::vector<Channel*> ChannelList;

  bool looping_;//描述事件循环是否正在循环中

  std::atomic<bool> quit_;//事件循环退出标记

  bool eventHandling_; //是否正在处理事件中

  bool callingPendingFunctors_;//是否正在处理任务池中的任务

  int64_t iteration_;//事件循环计数器：用处不大

  pid_t threadId_;//事件循环线程ID

  Timestamp pollReturnTime_;//获取poller监控返回时间

  std::unique_ptr<Poller> poller_;//事件监控器

  std::unique_ptr<TimerQueue> timerQueue_;//定时器

  int wakeupFd_;//唤醒事件循环的文件描述符

  std::unique_ptr<Channel> wakeupChannel_;

  std::any context_;//c++17 容器，用于存储任意类型的上下文数据

  ChannelList activeChannels_;//channel*数组 用于获取就绪channel

  Channel* currentActiveChannel_;//从activeChannels_中获取当前就绪channel的指针，用于处理事件

  std::mutex mutex_;//用于保护任务池的互斥锁

  std::vector<Functor> pendingFunctors_;//任务池，用于存储待执行的任务
};

}  // namespace net
