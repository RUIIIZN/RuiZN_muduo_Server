#pragma once

#include "../Poller.h"


struct epoll_event;

namespace net
{
class EPollPoller : public Poller
{
    public:
        EPollPoller(EventLoop* loop);
        ~EPollPoller() override;

        base::Timestamp poll(int timeoutMs, ChannelList* activeChannels) override;

        void updateChannel(Channel* channel) override;

        void removeChannel(Channel* channel) override;

    private:
        const char* operationToString(int op);

        void fillActiveChannels(int numEvents, ChannelList* activeChannels) const;

        void update(int operation, Channel* channel);

        static const int kInitEventListSize = 16;

        typedef std::vector<struct epoll_event> EventList;

        int epollfd_;
        
        EventList events_;
};
}