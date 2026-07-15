#pragma once

#include <set>
#include <vector>

#include "../channel/Channel.h"
#include "Timer.h"

namespace net
{

class EventLoop;
class Timer;
class TimerId;


int createTimerfd();//创建定时器fd

struct timespec howMuchTimeFromNow(Timestamp when);//计算when - now 的时间差 --- 有可能需要根据这个时间差重置定时器超时通知时间

void readTimerfd(int timerfd,Timestamp now);
//读取定时器数据 :
//当 timerfd 可读、Epoll 唤醒并调用 handleRead() 时，必须对这个 fd 进行 read 操作。
// 如果不读它，这个 fd 会一直保持可读状态

void resetTimerfd(int timerfd,Timestamp expiration);//重置定时器超时时间

class TimerQueue
{
 public:

  TimerQueue(EventLoop* loop);
  ~TimerQueue();

  TimerId addTimer(TimerCallback cb,Timestamp when,double interval);//添加定时任务
  void cancel(TimerId timerId);//取消定时任务

 private:

  typedef std::pair<Timestamp, Timer*> Entry;
  typedef std::set<Entry> TimerList;
  //先通过时间（Timestamp）排序 再通过唯一身份标识（sequence）排序


  typedef std::pair<Timer*, int64_t> ActiveTimer;
  typedef std::set<ActiveTimer> ActiveTimerSet;
  //活跃定时任务列表---红黑树管理
  //用于快速找到定时任务/判断当前定时任务是否在定时任务池中


  EventLoop* loop_;//所属的EventLoop

  int timerfd_;//定时fd

  Channel timerfdChannel_;//定时fd对应的ChannelTimestamp

  TimerList timers_;//定时任务列表---红黑树管理---定时任务池
  ActiveTimerSet activeTimers_;//活跃定时任务池
  ActiveTimerSet cancelingTimers_;//待取消定时任务池

  bool callingExpiredTimers_;//是否正在调用过期定时任务



  void addTimerInLoop(Timer* timer);//将添加操作抛入到所属loop线程peddingFunctor中
  void cancelInLoop(TimerId timerId);//将取消操作抛入到所属loop线程peddingFunctor中

  std::vector<Entry> getExpired(Timestamp now);//根据now当前时间获取所有过期定时任务

  void reset(const std::vector<Entry>& expireds, Timestamp now);//重置过期定时任务：针对 “重复性” 任务重新添加到定时任务池中

  bool insert(Timer* timer);//新增定时任务，并返回标识：是否需要重新设置定时器超时时间,详情->readme.md

  void handleRead();//timerfd 超时事件处理
};

}  // namespace net