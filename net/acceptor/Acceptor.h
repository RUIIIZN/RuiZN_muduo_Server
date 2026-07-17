#pragma once

#include <functional>

#include "../../base/log/Logging.h"
#include "../socket/Socket.h"
#include "../channel/Channel.h"
#include "../eventloop/EventLoop.h"

namespace net
{

class EventLoop;
class InetAddress;

using NewConnectionCallback = std::function<void (int , const InetAddress&)>;
///
/// Acceptor of incoming TCP connections.
///
class Acceptor 
{
public:

   Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport);
   ~Acceptor();

   void setNewConnectionCallback(const NewConnectionCallback& cb){ newConnectionCallback_ = std::move(cb); }

   void listen();

   bool listening() const { return listening_; }//是否正在监听

private:
   void handleRead();

   EventLoop* loop_;//所属的EventLoop --- baseloop
   Socket acceptSocket_; //监听套接字
   Channel acceptChannel_;//监听套接字的事件分发器
   NewConnectionCallback newConnectionCallback_;//新连接到来的回调函数
   bool listening_;//是否正在监听


   int idleFd_;//空闲文件描述符
   /*
   1. 为什么要有 idleFd_？（空闲文件描述符的妙用）
    在 Linux 网络编程中，有一个著名的 “文件描述符耗尽”（EMFILE） 灾难。如果服务器的 FD 满了，当新客户端连接来时，
    accept 会失败并返回 EMFILE。因为没有拿到 FD，这个连接在 TCP 队列里并没有被取出来，
    于是当前 Epoll 会不断触发可读事件，导致 CPU 100% 爆表卡死。

    idleFd_ 就是用来降维打击这个问题的：

    初始化时：先占坑打开一个空闲文件（比如 /dev/null），拿一个 FD。

    发生 EMFILE 时：把这个 idleFd_ 关掉（释放一个 FD 位置），赶紧调用 accept 把坑占满的那个新连接接过来，然后立刻断开它，
    最后重新打开 /dev/null 把坑踩回去。优雅防卡死！
   */
};

}  // namespace net
