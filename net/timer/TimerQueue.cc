#include <sys/timerfd.h>
#include <assert.h>

#include "TimerQueue.h"
#include"Timer.h"
#include "../eventloop/EventLoop.h"
#include "../../base/log/Logging.h"

namespace net
{
int createTimerfd()
{
  int timerfd = ::timerfd_create(CLOCK_MONOTONIC,TFD_NONBLOCK | TFD_CLOEXEC);
  if (timerfd < 0)
  {
    LOG_SYSFATAL << "Failed in timerfd_create";
  }
  return timerfd;
}

struct timespec howMuchTimeFromNow(Timestamp when)
{
  int64_t microseconds = when.microSecondsSinceEpoch()- Timestamp::now().microSecondsSinceEpoch();
  if (microseconds < 100)
  {
    microseconds = 100;
  }

  /*
  在 howMuchTimeFromNow 中，判断 microseconds < 100 并强制将其重置为 100 微秒，是一个非常经典且微妙的工程安全设计
  据 Linux 的 timerfd_settime 官方文档（man timerfd_settime）规定：
  如果 it_value 的秒数（tv_sec）和纳秒数（tv_nsec）同时为 0，那么这个定时器将会被“彻底关闭（disarmed）”！如果在计算 when - now 时：
  这个任务已经过期了（例如由于 CPU 调度延迟，计算结果为负数，或者刚好就是 0）；
  如果不加限制，算出来的 ts.tv_sec 和 ts.tv_nsec 就会被硬生生塞入 0。
  这样一来，本来应该“立刻、马上执行”的定时器，在传入 timerfd_settime 后，不但没有响，反而直接把底层的整个 timerfd 闹钟给关掉了！ 
  这会导致后面所有的定时任务全部失效卡死。强制设置为最小 100 微秒，就绝对避免了 (0, 0) 的出现。
  */


  struct timespec ts;
  ts.tv_sec = static_cast<time_t>(microseconds / Timestamp::kMicroSecondsPerSecond);
  ts.tv_nsec = static_cast<long>( (microseconds % Timestamp::kMicroSecondsPerSecond) * 1000 );
  return ts;
}

void readTimerfd(int timerfd, Timestamp now)
{
  uint64_t howmany;
  ssize_t ret = ::read(timerfd, &howmany, sizeof howmany);
  LOG_TRACE << "TimerQueue::handleRead() " << howmany << " at " << now.toFormatString();
  if (ret != sizeof howmany)
  {
    LOG_ERROR << "TimerQueue::handleRead() reads " << ret << " bytes instead of 8";
  }
}

void resetTimerfd(int timerfd, Timestamp expiration)
{
    // struct itimerspec its;
    // its.it_value = howMuchTimeFromNow(expiration);
    // int res = timerfd_settime(timerfd,0,&its,NULL);
    // if(res < 0)
    // {
    //     LOG_SYSERR << "resetTimerfd() Error";
    // }


    // wake up loop by timerfd_settime()
    struct itimerspec newValue;
    struct itimerspec oldValue;
    //memZero(&newValue, sizeof newValue);
    ::memset(&newValue, 0, sizeof newValue);
    //memZero(&oldValue, sizeof oldValue);
    ::memset(&oldValue, 0, sizeof oldValue);
    newValue.it_value = howMuchTimeFromNow(expiration);
    int ret = ::timerfd_settime(timerfd, 0, &newValue, &oldValue);
    if (ret)
    {
      LOG_SYSERR << "timerfd_settime()";
    }
}

};//namespace net


using namespace net;

TimerQueue::TimerQueue(EventLoop* loop)
  : loop_(loop),
    timerfd_(createTimerfd()),
    timerfdChannel_(loop, timerfd_),
    timers_(),
    callingExpiredTimers_(false)
{
  timerfdChannel_.setReadCallback(std::bind(&TimerQueue::handleRead, this));
  timerfdChannel_.enableReading();
}

TimerQueue::~TimerQueue()
{
  timerfdChannel_.disableAll();
  timerfdChannel_.remove();
  ::close(timerfd_);
  // do not remove channel, since we're in EventLoop::dtor();
  for (const Entry& timer : timers_)
  {
    delete timer.second;//释放定时任务池中的任务：还没有出发的定时任务
  }
}


TimerId TimerQueue::addTimer(TimerCallback cb,Timestamp when,double interval)
{
  Timer* timer = new Timer(std::move(cb), when, interval);
  loop_->runInLoop(std::bind(&TimerQueue::addTimerInLoop, this, timer));
  return TimerId(timer, timer->getsequence());
}

void TimerQueue::addTimerInLoop(Timer* timer)
{
  loop_->assertInLoopThread();
  bool earliestChanged = insert(timer);//这次插入是否改变了整个队列中最早过期的那个时间点

  if (earliestChanged)
  {
    resetTimerfd(timerfd_, timer->expiration());
  }
}

void TimerQueue::cancel(TimerId timerId)
{
  loop_->runInLoop(std::bind(&TimerQueue::cancelInLoop, this, timerId));
}

void TimerQueue::cancelInLoop(TimerId timerId)
{
  loop_->assertInLoopThread();
  assert(timers_.size() == activeTimers_.size());
  ActiveTimer timer(timerId.timer_, timerId.sequence_);//通过timerid拿到对应timer*和sequence
  ActiveTimerSet::iterator it = activeTimers_.find(timer); //在活跃定时任务池中查找该定时任务
  if (it != activeTimers_.end())//如果找到该定时任务
  {
    size_t n = timers_.erase(Entry(it->first->expiration(), it->first));//从定时任务池中删除该定时任务
    assert(n == 1); (void)n;//确保确实精准删除了一个
    delete it->first; //释放 Timer 实例内存（核心所有权销毁点）
    activeTimers_.erase(it);//从活跃定时任务池也中删除该定时任务
  }
  else if (callingExpiredTimers_)
  {
    cancelingTimers_.insert(timer);
  }
  assert(timers_.size() == activeTimers_.size());
}


std::vector<TimerQueue::Entry> TimerQueue::getExpired(Timestamp now)
{
  assert(timers_.size() == activeTimers_.size());
  std::vector<Entry> expired;
  Entry sentry(now, reinterpret_cast<Timer*>(UINTPTR_MAX));
  TimerList::iterator end = timers_.lower_bound(sentry);
  assert(end == timers_.end() || now < end->first);
  std::copy(timers_.begin(), end, back_inserter(expired));
  timers_.erase(timers_.begin(), end);

  for (const Entry& it : expired)
  {
    ActiveTimer timer(it.second, it.second->getsequence());
    size_t n = activeTimers_.erase(timer);
    assert(n == 1); (void)n;
  }

  assert(timers_.size() == activeTimers_.size());
  return expired;
}
/*
timers_ 里的元素是 Entry，即 std::pair<Timestamp, Timer*>。我们现在想在红黑树里找到所有小于或等于 now 的定时器。
在 std::set 中，lower_bound(key) 会返回 大于或等于 key 的第一个元素的迭代器。
如果我们仅仅用 now 去做 boundary：因为可能存在多个定时器的到期时间戳刚好一模一样（都是 now），
这些定时器的 Timer* 指针地址各不相同。如果我们想把所有到期时间 now 的元素一次性全找出来，
该怎么定这个查找边界（sentry）？

他创建了一个虚拟的“哨兵” sentry。
它的时间戳设为 now。
它的指针值设为了 UINTPTR_MAX（即 0xFFFFFFFF 或 0xFFFFFFFFFFFFFFFF，内存地址的物理极限最大值）。
根据 std::pair 的默认比较规则：先比第一个元素（时间戳），第一个元素相同再比第二个元素（指针地址）。
因为 sentry 的指针地址是最大的，所以：
任何实际存在的、时间戳同样为 now 的真实定时器，其 Entry(now, timer_ptr) 都要比 sentry(now, UINTPTR_MAX) 小！
*/



void TimerQueue::reset(const std::vector<Entry>& expired, Timestamp now)//重置过期的定时任务并重置timerfd过期时间
{
  for (const Entry& it : expired)
  {
    ActiveTimer timer(it.second, it.second->getsequence());
    if (it.second->repeated() && cancelingTimers_.find(timer) == cancelingTimers_.end())
    {
      it.second->restart(now);
      insert(it.second);
    }
    else
    {
      delete it.second;
    }
  }

  Timestamp nextExpire;

  if (!timers_.empty())
  {
    nextExpire = timers_.begin()->second->expiration();
  }

  if (nextExpire.valid())
  {
    resetTimerfd(timerfd_, nextExpire);
  }
}

void TimerQueue::handleRead()
{
  loop_->assertInLoopThread();
  Timestamp now(Timestamp::now());
  readTimerfd(timerfd_, now);

  std::vector<Entry> expired = getExpired(now);

  callingExpiredTimers_ = true;
  cancelingTimers_.clear();
  // safe to callback outside critical section
  for (const Entry& it : expired)
  {
    it.second->run();
  }
  callingExpiredTimers_ = false;

  reset(expired, now);
}

bool TimerQueue::insert(Timer* timer)
{
  loop_->assertInLoopThread();
  assert(timers_.size() == activeTimers_.size());
  bool earliestChanged = false;
  Timestamp when = timer->expiration();
  TimerList::iterator it = timers_.begin();
  if (it == timers_.end() || when < it->first)
  {
    earliestChanged = true;
  }

  {
    std::pair<TimerList::iterator, bool> result = timers_.insert(Entry(when, timer));
    assert(result.second); (void)result;
  }

  {
    std::pair<ActiveTimerSet::iterator, bool> result
      = activeTimers_.insert(ActiveTimer(timer, timer->getsequence()));
    assert(result.second); (void)result;
  }

  assert(timers_.size() == activeTimers_.size());
  return earliestChanged;
}