#include "../../base/log/Logging.h"
#include "Channel.h"
#include <assert.h>

#include <sstream>

#include <sys/epoll.h>


using namespace net;

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;

Channel::Channel(EventLoop* loop, int fd__)
  : loop_(loop),
    fd_(fd__),
    events_(0),
    revents_(0),
    index_(-1),
    logHup_(true),
    tied_(false),
    eventHandling_(false),
    addedToLoop_(false)
{}

Channel::~Channel()
{
  assert(!eventHandling_);
  assert(!addedToLoop_);
  // if (loop_->isInLoopThread())
  // {
  //   assert(!loop_->hasChannel(this));
  // }
}

void Channel::tie(const std::shared_ptr<void>& obj)
{
  tie_ = obj;
  tied_ = true;
}

void Channel::update()//将Channel添加到poller管理中
{
  addedToLoop_ = true;
  loop_->updateChannel(this);
}

void Channel::remove()
{
  assert(isNoneEvent());
  addedToLoop_ = false;
  loop_->removeChannel(this);
}

void Channel::handleEvent(Timestamp receiveTime)
{
  std::shared_ptr<void> guard;
  if (tied_)
  {
    guard = tie_.lock();
    if (guard)
    {
      handleEventWithGuard(receiveTime);
    }
  }
  else
  {
    handleEventWithGuard(receiveTime);
  }
}

//EPOLLHUP 链接断开
//EPOLLRDHUP 半关闭 ：shutdown_RD/WR 
void Channel::handleEventWithGuard(Timestamp receiveTime)
{
  eventHandling_ = true;
  LOG_TRACE << reventsToString();
  if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN))
  {
    if (logHup_)
    {
      LOG_WARN << "fd = " << fd_ << " Channel::handle_event() POLLHUP";
    }
    if (closeCallback_) closeCallback_();
  }

  // if (revents_ & POLLNVAL)
  // {
  //   LOG_WARN << "fd = " << fd_ << " Channel::handle_event() POLLNVAL";
  // }

  if (revents_ & (EPOLLERR))
  {
    if (errorCallback_) errorCallback_();
  }
  if (revents_ & (EPOLLIN | EPOLLPRI | EPOLLRDHUP))
  {
    if (readCallback_) readCallback_(receiveTime);
  }
  if (revents_ & EPOLLOUT)
  {
    if (writeCallback_) writeCallback_();
  }
  eventHandling_ = false;
}

std::string Channel::reventsToString() const
{
  return eventsToString(fd_, revents_);
}

std::string Channel::eventsToString() const
{
  return eventsToString(fd_, events_);
}

std::string Channel::eventsToString(int fd, int ev)
{
  std::ostringstream oss;
  oss << fd << ": ";
  if (ev & EPOLLIN)
    oss << "IN ";
  if (ev & EPOLLPRI)
    oss << "PRI ";
  if (ev & EPOLLOUT)
    oss << "OUT ";
  if (ev & EPOLLHUP)
    oss << "HUP ";
  if (ev & EPOLLRDHUP)
    oss << "RDHUP ";
  if (ev & EPOLLERR)
    oss << "ERR ";
  // if (ev & EPOLLNVAL)
  //   oss << "NVAL ";

  return oss.str();
}