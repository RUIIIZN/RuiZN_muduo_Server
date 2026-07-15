#pragma once

#include <cstdint>

namespace net
{

class Timer;

class TimerId 
{
 public:
  TimerId()
    : timer_(nullptr),
      sequence_(0)
  {}

  TimerId(Timer* timer, int64_t seq)
    : timer_(timer),
      sequence_(seq)
  {}

  friend class TimerQueue;//在TimerQueue中使用TimerId的私有成员

 private:
    Timer* timer_;
    int64_t sequence_;//唯一身份标识
};

}  // namespace net