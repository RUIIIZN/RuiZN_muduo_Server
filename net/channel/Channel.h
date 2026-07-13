#pragma once

namespace net
{
    
class EventLoop;
class Channel
{
    public:
        Channel(int fd);
        ~Channel() = default;
        int index() const { return index_; }
        int fd() const { return fd_; }
        int events() const { return events_; }
        void setrevents(int revt) { revents_ = revt; }
    private:
        int fd_;
        int events_;
        int revents_;
        int index_;//状态索引
        bool eventHandling_;
        bool addedToLoop_;
        EventLoop* loop_;
};

}