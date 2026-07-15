#include <assert.h>
#include <unistd.h>
#include "EPollPoller.h"
#include "../../../base/log/Logging.h"

class Channel;

using namespace base;

namespace net
{
    int createEpoll()//创建epoll文件描述符
    {
        int epollfd = ::epoll_create1(EPOLL_CLOEXEC);
        if (epollfd < 0)
        {
            LOG_FATAL << "createEpoll failed";
        }
        return epollfd;
    }

    EPollPoller::EPollPoller(EventLoop* loop) 
      : Poller(loop),
        epollfd_(createEpoll()),
        events_(kInitEventListSize)
        {}

    EPollPoller::~EPollPoller() 
    {
        ::close(epollfd_);
    }
    Timestamp EPollPoller::poll(int timeoutMs, ChannelList* activeChannels) //开始轮询事件
    {
        int res = ::epoll_wait(epollfd_,&events_[0],events_.size(),timeoutMs);
        if (res < 0)
        {
            LOG_ERROR << "epoll_wait error";
            return Timestamp::now();
        }
        else if(res == 0)
        {
            LOG_TRACE << "epoll_wait timeout";
            return Timestamp::now();
        }
        else
        {
            fillActiveChannels(res,activeChannels);
            if (res == static_cast<int>(events_.size()))
            {
                events_.resize(events_.size()*2);
            }
            return Timestamp::now();
        }
    }

    void EPollPoller::updateChannel(Channel* channel) //将Channel添加到poller管理中
    {
        // 针对channel的index状态进行不同处理:
        // KNew 尚未在事件监听器
        // KDeleted 已从事件监听器中移除，但仍然存在于 Poller 的内部管理结构中(内核删除了，应用层还存在)
        // KAdded 已成功注册到事件监听器中，正在被监控
        if(channel->index() == kNew || channel->index() == kDeleted)
        {
            if(channel->index() == kNew)
            {
                assert(channels_.find(channel->fd()) == channels_.end());
                channels_[channel->fd()] = channel;
            }
            else
            {
                assert(channels_.find(channel->fd()) != channels_.end());
                assert(channels_[channel->fd()] == channel);
            }
            update(EPOLL_CTL_ADD,channel);
            channel->set_index(kAdded);
        }
        else
        {
            if(channel->isNoneEvent())
            {
                update(EPOLL_CTL_DEL,channel);
                channel->set_index(kDeleted);
            }
            else
            {
                update(EPOLL_CTL_MOD,channel);
            }
        }
    }

    void EPollPoller::removeChannel(Channel* channel) //将Channel从poller管理中移除
    {
        assert(channels_.find(channel->fd()) != channels_.end());
        assert(channels_[channel->fd()] == channel);
        if(channel->index() == kAdded)
        {
            update(EPOLL_CTL_DEL,channel);
        }
        channels_.erase(channel->fd());
        channel->set_index(kNew);
    }

    const char* EPollPoller::operationToString(int op)//将操作码转换为字符串
    {
        switch (op)
        {
            case EPOLL_CTL_ADD:
                return "ADD";
            case EPOLL_CTL_DEL:
                return "DEL";
            case EPOLL_CTL_MOD:
                return "MOD";
            default:
                return "UNKNOWN";
        }
    }

    void EPollPoller::fillActiveChannels(int numEvents, ChannelList* activeChannels) const//将所有活跃的Channel添加到activeChannels中
    {
        for (int i = 0; i < numEvents; ++i)
        {
            Channel* channel = static_cast<Channel*>(events_[i].data.ptr);
            channel->set_revents(events_[i].events);
            activeChannels->push_back(channel);
        }
    }

    void EPollPoller::update(int operation, Channel* channel)//将Channel添加到poller管理中
    {
        int fd = channel->fd();
        int events = channel->events();
        struct epoll_event ev;
        ev.data.ptr = channel;
        /*
        这是一个非常重要的设计！epoll_event 的 data 是一个联合体（union），可以放 ptr 或 fd。
        muduo 使用 ptr 直接指向 Channel 对象，这样当 epoll_wait 返回时，可以直接通过 ptr 拿到对应的 Channel，
        调用其回调函数，无需通过 fd 再查找一次，效率更高。
        */
        ev.events = events;
        int res = epoll_ctl(epollfd_,operation,fd,&ev);
        if(res < 0)
        {
            LOG_ERROR << "epoll_ctl error";
        }

    }
}