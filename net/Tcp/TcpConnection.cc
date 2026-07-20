#include "TcpConnection.h"
#include "../socket/Socket.h"
#include "../channel/Channel.h"

#include <string>
#include <assert.h>


using namespace net;
class Channel;
class EventLoop;
class Socket;


TcpConnection::TcpConnection(EventLoop* loop,
                const std::string& name,
                int sockfd,const InetAddress& localAddr,
                const InetAddress& peerAddr):
    name_(name),
    loop_(loop),
    state_(KConnecting),
    socket_(new Socket(sockfd)),
    localAddr_(localAddr),
    peerAddr_(peerAddr),
    reading_(true),
    channel_(new Channel(loop, sockfd)),
    highWaterMark_(1)
{
    //设置套接字
    socket_->setKeepAlive(true);
    channel_->setReadCallback(std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
    channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(std::bind(&TcpConnection::handleError, this));
    LOG_DEBUG << "constructor TcpConnection " << name_ << " sockfd = " << socket_->fd();
}

TcpConnection::~TcpConnection()
{
    LOG_DEBUG << "destructor TcpConnection " << name_ << " sockfd = " << socket_->fd();
    assert(state_ == KDisconnected);
}

//  bool TcpConnection::getTcpInfo(struct tcp_info* tcpi) const
//  {
//     return socket_->getTcpInfo(tcpi);
//  }

// std::string TcpConnection::getTcpInfoString() const
// {
//     char buf[1024];
//     buf[0] = '\0';
//     socket_->getTcpInfoString(buf, sizeof buf);
//     return buf;
// }

void TcpConnection::send(Buffer* message)
{
    send(static_cast<const void*>(message->peek()), message->readableBytes());
}

void TcpConnection::send(const void* message, int len)
{
    //如果在所属ioloop直接发送，若不在跨线程投递
    if (loop_->isInLoopThread())
    {
        sendInLoop(message, len);
    }
    else
    {
        std::string msg((char*)message, len);//此函数是非阻塞的，如果不保存数据可能把任务添加进去后，message 被释放了
        void (TcpConnection::* funcp)(const std::string&) = &TcpConnection::sendInLoop;
        loop_->runInLoop(std::bind(funcp,this,msg));
    }

}

//解决send线程安全问题，必须在所属的ioLoop中进行send
void TcpConnection::sendInLoop(const std::string& message)
{
    send(static_cast<const void*>(message.c_str()), message.size());
}
void TcpConnection::sendInLoop(const void* message, size_t len)
{
    //1.如果 当前fd没有写事件监控 且 发送缓冲区没有数据 直接系统调用 write/send 发送数据
    //否则 将数据放入到发送缓冲区中

    //2.如果直接发送数据。但是数据没有发送完，将剩余数据放入到用户态的outputBuffer 中
    //并 开启写事件监控

    // 为什么平时“不能”一直开启写事件监控？为什么不从一开始就让 Epoll 监控 connfd 的写事件？

    // 在水平触发（LT，Level Triggered）模式下：
    // 只要内核的 TCP 发送缓冲区未满（即还可以写入数据），Epoll 就会一直不停地触发可写事件（EPOLLOUT）。
    // 如果你平时不发数据、或者数据早就发完了，却还一直开启着写事件监控，那么每次 epoll_wait 都会瞬间带着可写事件返回。
    // 后果：你的 EventLoop 线程会被这些无意义的可写事件完全占满，CPU 瞬间飙到 100% 发生空转暴毙。
    // 所以，Muduo 采用的是极其精准的“按需开启”策略：
    // 只有当“一次性没发完，需要后续异步发送”时，才开启写事件监控（enableWriting()）。
    // 一旦 outputBuffer_ 中的积压数据被全部排空，立刻关闭写事件监控（disableWriting()），让 CPU 重新归于平静。

    loop_->assertInLoopThread();
    bool faultError = false;//致命错误标记
    ssize_t nwrote = 0;//本次发送的字节数
    size_t remaining = len;//剩余要发送的字节数

    if (state_ == KDisconnected)//如果连接已经断开 则直接返回
    {
        LOG_WARN << "disconnected, give up writing";
        return;
    }

    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        nwrote = sockets::write(channel_->fd(), message, len);
        if (nwrote >= 0)
        {
            remaining = len - nwrote;
            if (remaining == 0 && writeCompleteCallback_)
            {
                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
        }
        else // nwrote < 0
        {
            nwrote = 0;
            if (errno != EWOULDBLOCK)//非阻塞模式下 发送失败 且 错误不是 EWOULDBLOCK(发送缓冲区已满) 则是致命错误
            {
                LOG_SYSERR << "TcpConnection::sendInLoop : error" ;
                if (errno == EPIPE || errno == ECONNRESET)
                {
                faultError = true;
                }
            }
        }
    }
    assert(remaining <= len);
    if (faultError == false && remaining > 0)
    {
        size_t oldLen = outputBuffer_.readableBytes();//当前发送缓冲区上次剩余旧的字节数
        if (oldLen + remaining >= highWaterMark_
             && oldLen < highWaterMark_
               && highWaterMarkCallback_)//如果积压的字节数和本次期望发送的数据字节数大于等于 高水位线 则触发 高水位线回调
        {
            loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));
        }
        outputBuffer_.append(static_cast<const char*>(message)+nwrote, remaining);//将剩余数据放入到发送缓冲区中
        if (!channel_->isWriting())//为fd注册写事件监控
        {
            channel_->enableWriting();
        }
    }

}


// 关闭连接操作
// void TcpConnection::shutdown()
// {
//     if (state_ == KConnected)
//     {
//      setState(KDisconnecting);
//      loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, shared_from_this()));
//     }

// }

// void TcpConnection::shutdownInLoop()
// {
//     loop_->assertInLoopThread();
//     if (!channel_->isWriting())
//     {
//       socket_->shutdownWrite();
//     }
// }

void TcpConnection::forceClose()
{
    if (state_ == KConnected || state_ == KDisconnecting)
    {
        setState(KDisconnecting);
        loop_->queueInLoop(std::bind(&TcpConnection::forceCloseInLoop, shared_from_this()));
    }
}
// void TcpConnection::forceCloseWithDelay(double seconds)
// {
//     if (state_ == KConnected || state_ == KDisconnecting)
//     {
//         setState(KDisconnecting);
//         loop_->runAfter(
//         seconds,
//         makeWeakCallback(shared_from_this(),
//             &TcpConnection::forceClose));
//     }
// }

void TcpConnection::forceCloseInLoop()
{
    loop_->assertInLoopThread();
    if (state_ == KConnected || state_ == KDisconnecting)
    {
        //KConnected 包含以下几种情况：
        //1.心跳超时（应用层踢人） 2.服务器关闭/过载保护 3.业务层逻辑冲突（多处登录）
        //4.客户端异常断  5.恶意连接清理
        handleClose();
    }
}

void TcpConnection::setTcpNoDelay(bool on)
{
    socket_->setTcpNoDelay(on);
}


void TcpConnection::startRead()
{
    loop_->runInLoop(std::bind(&TcpConnection::stopReadInLoop, this));
}
void TcpConnection::stopRead()
{
    loop_->assertInLoopThread();
    if (reading_ || channel_->isReading())
    {
        channel_->disableReading();
        reading_ = false;
    }
}

void TcpConnection::connectEstablished() //一旦connection 对象全都完成了 连接就绪时 就执行的接口
{
    //连接对象的所有初始化完成后调用的回调函数，必须在ioloop中 ---SubEventLoop
    loop_->assertInLoopThread();
    assert(state_ == KConnecting);
    setState(KConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading();

    if (connectionCallback_) connectionCallback_(shared_from_this());
    /*
    正确的理解：
        MainLoop（TcpServer） 创建了连接，存入 connections_ 账本中（此时计数为 2，Map 里一个，投递的任务队列里一个）。
        主线程把任务丢给 SubLoop 后，主线程就去忙别的了。
        SubLoop 线程从队列里取出任务，开始执行 connectEstablished()。此时任务包销毁，世界的唯一强引用只剩 TcpServer 
        的 Map 账本了，计数降为 1。
        如果你在 connectEstablished() 里调用用户回调时，传入了裸指针 this：connectionCallback_(*this);。
        此时控制权进入用户的 onConnection() 函数。用户在函数内部发现该连接在黑名单里，于是调用了 
        server_->removeConnection(conn.name());。
        关键点来了： TcpServer::removeConnection 为了保证线程安全，通常会把真正的删除动作通过 runInLoop 
        转交给 MainLoop 线程去从 Map 里 erase。
        一旦 MainLoop 线程把 Map 里的 shared_ptr 删除了，整个世界对这个连接的强引用计数瞬间从 1 降到了 0！
        堆上的 TcpConnection 内存被系统瞬间回收（释放）。
        当 SubLoop 里的用户业务代码跑完，控制权准备高高兴兴从 connectionCallback_ 返回到 
        connectEstablished() 的下一行代码时，SubLoop忽然发现：糟糕，我的 this 指针指向的整个对象已经当场暴毙了！  
        接下来的任何操作直接触发野指针踩空崩溃（Core Dump）。

        ERROR  自己的理解有错误：
        必须传shared_from_this()  而不是this!!!
        因为TcpConnection对象是在mainloop中也就是tcpserver中管理的，
        当tcpserver---main reactor 在执行newConnection的过程中当在自己的管理册中注册完后Ser
        会将这个新连接对象分发给subreactor，让sub去执行connectEstablished()函数
        TcpServer会去执行onConnection()函数 进行用户自定义的业务处理
        此时这个连接的强引用计数为1，--- 在TcpServer的管理策中
        如果subreactor在向自己的poller中注册这个channel的时候传入连接对象的裸指针
        与此同时TcpServer在onConnection中执行了例如判断连接是否处于黑名单的业务逻辑并且刚好在黑名单里
        这时候TcpServer就会调用removeConnection()函数 移除这个连接对象 ，连接对象的引用计数-1
        此时SubEventLoop中的连接对象引用计数为0 对象被释放了，那么在sub中再去操作这个对象就会出现野指针访问！
    */
}
void TcpConnection::connectDestroyed()
{
    loop_->assertInLoopThread();
    if (state_ == KConnected)
    {
        setState(KDisconnected);
        channel_->disableAll();

        connectionCallback_(shared_from_this());//调用用户注册的连接销毁回调函数
    }
    channel_->remove();

    /*
        为什么handleclose 和 connectDestroyed 里都有 connectioncallback？

        正常走完一生（寿终正寝）：handleClose() 修改状态为 kDisconnected 并通知用户 connectDestroyedco() 
        判定状态不匹配，跳过通知，直接火化。
        遭遇不可抗力（飞来横祸）：绕过 handleClose()  connectDestroyed() 发现状态依然是 kConnected，
        赶紧触发 if 块里的紧急备用通道，先帮它把状态改了、把 Epoll 撤了、把断开通知补上，最后再火化。
    */

    /*
        连接对象的释放有三种场景：
        1.连接断开的时候 ：hanldeClose（state == KConnected）：解除监控---设置状态为KDisconnected---调用connectionCallback---调用closeCallback
        2.用户调用foreceClose的时候：foreceClose（state == KDisconnecting）--- handleClose（state == KDisconnecting）
        3.TcpServer析构的时候：调用conn->connectDestroyed (state == KConnected)
    */
}

void TcpConnection::handleRead(base::Timestamp receiveTime)//设置channel 的 读事件回调
{
    loop_->assertInLoopThread();
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
    if (n > 0)
    {
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }
    else if (n == 0)
    {
        //连接断开
        handleClose();
    }
    else
    {
        errno = savedErrno;
        LOG_SYSERR << "TcpConnection::handleRead";
        handleError();
    }
}
void TcpConnection::handleWrite()//设置channel 的 写事件回调
{
    loop_->assertInLoopThread();
    if (channel_->isWriting())
    {
        ssize_t n = sockets::write(channel_->fd(),
                               outputBuffer_.peek(),
                               outputBuffer_.readableBytes());
        if (n > 0)
        {
            outputBuffer_.retrieve(n);
            if (outputBuffer_.readableBytes() == 0)
            {
                channel_->disableWriting();
                if (writeCompleteCallback_)
                {
                    loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
                }
                if (state_ == KDisconnecting)
                {
                    forceCloseInLoop();
                }
            }
        }
        else
        {
            LOG_SYSERR << "TcpConnection::handleWrite";
            // if (state_ == kDisconnecting)
            // {
            //   shutdownInLoop();
            // }
        }
    }
    else
    {
        LOG_TRACE << "Connection fd = " << channel_->fd()
              << " is down, no more writing";
    }
}

void TcpConnection::handleClose()//设置channel 的 关闭事件回调 --- 实际的资源释放函数
{
    loop_->assertInLoopThread();
    LOG_TRACE << "fd = " << channel_->fd() << " state = " << stateToString();
    assert(state_ == KConnected || state_ == KDisconnecting);

    setState(KDisconnected);
    channel_->disableAll();//删除fd 的 所有事件监控

    TcpConnectionPtr guardThis(shared_from_this());//额外保存一份智能指针 用于在回调函数中使用  防止连接对象被销毁
    if (connectionCallback_)
    {
        /*
        guardThis传给用户注册的连接事件回调函数。用户通常会在对应的 onConnection 业务函数里写:

        if (!conn->connected()) 
        LOG_INFO << "客户端断开，清理业务数据";
        这里可以使用 conn（也就是 guardThis）来获取 fd、底层身份信息等
        */
        connectionCallback_(guardThis);
    }

    // must be the last line
    if (closeCallback_)
    {
        //把当前连接从 TcpServer 的 ConnectionMap（std::map）中彻底移除
        //把当前连接内部的 Channel 从子线程的 Poller（Epoll）中彻底注销
        //将最后的清理任务通过 loop_->queueInLoop 派发到最后阶段（通常是异步调用 TcpConnection::connectDestroyed 做最后的资源释放）
        closeCallback_(guardThis);
    }
}
void TcpConnection::handleError()//设置channel 的 错误事件回调
{
    LOG_SYSERR << "TcpConnection::handleError -> fd:" << channel_->fd();
}

const char* TcpConnection::stateToString() const
{
    switch (state_)
    {
     case KDisconnected:
        return "kDisconnected";
     case KConnecting:
        return "kConnecting";
     case KConnected:
        return "kConnected";
     case KDisconnecting:
        return "kDisconnecting";
     default:
        return "unknown state";
    }
}
void TcpConnection::startReadInLoop()
{
    loop_->runInLoop(std::bind(&TcpConnection::startReadInLoop, this));
}
void TcpConnection::stopReadInLoop()
{
    loop_->assertInLoopThread();
    if (!reading_ || !channel_->isReading())
    {
        channel_->enableReading();
        reading_ = true;
    }
}
