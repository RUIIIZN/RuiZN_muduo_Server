#include "Poller.h"

#include "../poller/Poller.h"
#include "epollpoller/EPollPoller.h"

using namespace net;

Poller::Poller(EventLoop* loop)
  : ownerLoop_(loop)
{}

Poller::~Poller() = default;

bool Poller::hasChannel(Channel* channel) const
{
  ChannelMap::const_iterator it = channels_.find(channel->fd());
  return it != channels_.end() && it->second == channel;
}
Poller* Poller::newDefaultPoller(EventLoop* loop)
{
  return new EPollPoller(loop);
}