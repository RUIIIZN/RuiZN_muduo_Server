#pragma once

#include<vector>
#include<map>

//#include "muduo/net/EventLoop.h"
#include "../../base/time/Timestamp.h"
#include "../channel/Channel.h"

namespace net
{

class EventLoop;

const int kTimeoutDefaultMs = 1000;
const int kNew = -1;
const int kAdded = 1;
const int kDeleted = 2;

class Poller
{
    public:
        typedef std::vector<Channel*> ChannelList;
        Poller(EventLoop* loop);
        virtual ~Poller();
        virtual net::Timestamp poll(int timeoutMs, ChannelList* activeChannels) = 0;
        virtual void updateChannel(Channel* channel) = 0;
        virtual void removeChannel(Channel* channel) = 0;
        virtual bool hasChannel(Channel* channel) const;
        static Poller* newDefaultPoller(EventLoop* loop);
    protected:
        typedef std::map<int, Channel*> ChannelMap;
        ChannelMap channels_;
    private:
        EventLoop* ownerLoop_;
};

}