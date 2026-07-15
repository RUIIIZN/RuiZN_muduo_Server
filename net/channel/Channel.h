#pragma once

#include <functional>
#include <memory>
#include "../../base/time/Timestamp.h"
#include "../eventloop/EventLoop.h"

using base::Timestamp;
namespace net
{

typedef std::function<void()> EventCallback;
typedef std::function<void(Timestamp)> ReadEventCallback;

class Channel
{
    public:
        Channel(EventLoop* loop, int fd);
        ~Channel();

        void handleEvent(Timestamp receiveTime);//处理channel的事件
        void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }//设置channel的读事件回调函数
        void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }//设置channel的写事件回调函数
        void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }//设置channel的关闭事件回调函数
        void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }//设置channel的错误事件回调函数
        
        void tie(const std::shared_ptr<void>& obj);//通过观察tieptr观察外部对象是否被释放
        int fd() const { return fd_; }//获取channel的fd
        int events() const { return events_; }//获取channel的事件类型
        void set_revents(int revt) { revents_ = revt; }//设置channel的事件类型
        bool isNoneEvent() const { return events_ == kNoneEvent; }//是否没有事件监控

        void enableReading() { events_ |= kReadEvent; update(); }//启动channel的读事件监控
        void disableReading() { events_ &= ~kReadEvent; update(); }//禁用channel的读事件监控
        void enableWriting() { events_ |= kWriteEvent; update(); }//启动channel的写事件监控
        void disableWriting() { events_ &= ~kWriteEvent; update(); }//禁用channel的写事件监控
        void disableAll() { events_ = kNoneEvent; update(); }//禁用channel的所有事件监控
        bool isWriting() const { return events_ & kWriteEvent; }//是否在写事件监控
        bool isReading() const { return events_ & kReadEvent; }//是否在读事件监控

        
        int index() const { return index_; }//获取channel状态
        void set_index(int idx) { index_ = idx; }//设置channel的状态
        
        // for debug
        std::string reventsToString() const;
        std::string eventsToString() const;

        void doNotLogHup() { logHup_ = false; }

        EventLoop* ownerLoop() { return loop_; }//返回channel所属的EventLoop
        void remove();

    private:

        static std::string eventsToString(int fd, int ev);//将事件类型转换为字符串
        void update();//更新channel的状态
        void handleEventWithGuard(Timestamp receiveTime);//处理channel的事件

        static const int kNoneEvent;//没有事件监控
        static const int kReadEvent;//读事件监控
        static const int kWriteEvent;//写事件监控

        EventLoop* loop_;   //所属的EventLoop
        const int  fd_;     //channel的fd
        int        events_; //channel的注册事件类型
        int        revents_;//channel的实际就绪事件类型
        int        index_;  //当前channel是否在poller中添加了监控epollctl---内核空间
        bool       logHup_; //是否日志记录HUP事件

        std::weak_ptr<void> tie_;//指向外部管理对象，比如： Acceptor，Connection，TimerQueue
        bool tied_;              //是否设置了外部管理对象
        bool eventHandling_;     //它是用来在析构函数中进行安全断言（Assertion）的，目的是防止 Channel 对象在执行回调函数（事件处理中）时被意外销毁。
        bool addedToLoop_;       //状态描述：当前channel是否在poller管理中是否在ChannelMap中---用户空间

        EventCallback writeCallback_;   //写事件回调函数
        EventCallback closeCallback_;   //关闭回调函数
        EventCallback errorCallback_;   //错误回调函数
        ReadEventCallback readCallback_;//读事件回调函数
        /*
        读事件回调函数要传入时间戳
        当读时间就绪后：1.接收数据 --- 取消超时关闭 --- 2.业务处理（可能很耗时）--- 重新添加超时关闭 --- 3.响应数据
        防止在业务处理时关闭了链接
        */

};

}