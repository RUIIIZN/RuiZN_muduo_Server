#pragma once

#include "../buffer/Buffer.h"
#include "../inetaddress/InetAddress.h"
#include "../../base/time/Timestamp.h"

#include <memory>
#include <any>
#include <string>
#include <functional>


// struct tcp_info is in <netinet/tcp.h>
struct tcp_info;


namespace net
{

class Channel;
class EventLoop;
class Socket;

/*
    不用recv：因为我们将fd内核空间数据拷贝到buffer中的操作通过回调函数执行，
    也就是当connfd可读时，poller中会执行回调函数，将数据拷贝到其对应的buffer中。

    1. 传统多线程 / 阻塞模式（“主动拉取”数据）
    在传统的 thread-per-connection（一个连接一个线程）模型中，每个线程都在阻塞等待：

    // 传统模式：线程卡在这里，主动去调用 recv 试图“拉取”数据
    char buf[1024];
    int n = ::recv(connfd, buf, sizeof(buf), 0);

    2. Muduo 的 Reactor 模式（“被动通知” + 自动拷贝）
    在 TcpConnection 中，我们不需要在业务层手动去调用 recv。因为这一步被高度自动化地封装成了事件回调。

    当一个 TcpConnection 建立时，会发生以下连锁反应：

    1.阶段 A：注册“可读”事件
    每个 TcpConnection 内部都持有一个 Socket 和一个对应的 Channel。
    TcpConnection 会向它的 Channel 注册可读回调函数（TcpConnection::handleRead）。
    Channel 会被加入到子线程（SubLoop）的 Poller 中，监听 EPOLLIN（可读事件）。

    阶段 B：数据到来，自动拷贝到 Buffer
    客户端发送数据，内核 TCP 接收缓冲区收到数据，Poller（Epoll）检测到 connfd 可读，并返回给 EventLoop。
    EventLoop 调用该 Channel 绑定的可读回调函数：TcpConnection::handleRead。
    在 handleRead 内部，网络库会自动将内核缓冲区的数据拷贝到应用层的 inputBuffer_ 中
*/

/*
    关于send与force close

    send在发送数据的时候可能是将send任务添加到了eventloop的任务池中还没有执行，
    如果这时候close必须被添加到任务池中进行执行，不能直接关闭连接，不然就会出现数据丢失

    send --- runInLoop --- 如果在所在io线程中，直接发送数据 如果不在就 queueInLoop 跨线程投递到任务池
    force Close --- queueInLoop 必须将任务添加到任务池中，不能直接关闭连接
*/

/*
    public std::enable_shared_from_this<TcpConnection>

    允许 TcpConnection 在其内部的成员函数中，安全地获取指向当前对象（this）的 std::shared_ptr
    （即强引用计数智能指针），从而在异步多线程回调中延长自己的生命周期，
    防止程序因访问已被销毁的对象而发生段错误（Segment Fault）崩溃。

    1. 经典痛点：多线程异步回调中的“对象提前死亡”
    在 Muduo 架构中，TcpConnection 对象的生命周期是非常不确定的。它由主线程的 TcpServer 管理，
    但它的读写事件是由子线程（SubLoop）的 Poller 驱动的。

    假设一种极其常见的多线程竞态场景：

    某个客户端的 connfd 触发了可读事件，子线程正准备去调用 TcpConnection::handleRead。

    恰在此刻，主线程因为某种业务逻辑（或者客户端突然断开）决定关闭这个连接，于是把这个 TcpConnection 
    从 TcpServer 的 map 容器中移除了（强引用计数减 1 归零，对象被 delete 销毁）。

    此时，子线程如果继续拿着一个裸指针（this）去执行 handleRead，访问的将是一块已经被释放的非法内存，
    程序瞬间崩溃（Core dump）。    

    2. 解决方案：用 shared_from_this() 绑定回调 --- TcpConnection 对象内部其实偷偷隐藏了一个弱引用指针（std::weak_ptr）
    为了让子线程在执行事件回调时，TcpConnection 对象绝对活着，我们在给底层的 Channel 绑定回调函数时，
    不能传入裸指针 this，而必须传入一个强引用的 shared_ptr。

    继承了 enable_shared_from_this 后，类内部就可以调用 shared_from_this() 方法：

    它是如何保护生命周期的？
    当 std::bind 绑定了 shared_from_this() 返回的智能指针后，这个智能指针就会被拷贝并存放在 Channel 的内部成员变量中。

    只要这个 Channel 没有被销毁，或者这个回调函数没有被重置，TcpConnection 的引用计数就至少为 1。

    即使上层 TcpServer 把该连接从 std::map 中剔除了，由于子线程的 Channel 里还死死攥着一个强引用智能指针，
    TcpConnection 对象在内存中依然完好无损。

    直到子线程把所有的读写回调函数全部安全地执行完毕，Channel 被移除，智能指针出栈销毁，引用计数归零，
    TcpConnection 才会最终在子线程中被优雅地析构。

    3. 为什么不能直接 std::shared_ptr<TcpConnection>(this)？
    你可能会问，既然需要智能指针，为什么不直接在成员函数里写 std::shared_ptr<TcpConnection>(this) 呢？

    这是 C++ 智能指针中非常致命的禁忌！
    如果你直接用裸指针 this 强行构造一个新的 shared_ptr：

    这会导致两个完全独立、互不相干的智能指针控制块（Control Block）去管理同一块内存。它们各自以为自己是唯一的管理者，
    从而在引用计数归零时，分别对 this 内存调用一次 delete，引发二次释放（Double Free）导致程序直接崩溃。

    而继承 std::enable_shared_from_this 并使用 shared_from_this()，它会默默查找外部已经存在的那个控制块，
    让引用计数在同一个控制块下安全地加 1，绝对不会引发 Double Free。
*/
class TcpConnection;

using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
using ConnectionCallback = std::function<void(TcpConnectionPtr)>;
using MessageCallback = std::function<void(TcpConnectionPtr, Buffer*, base::Timestamp)>;
using WriteCompleteCallback = std::function<void(TcpConnectionPtr)>;
using HighWaterMarkCallback = std::function<void(TcpConnectionPtr, size_t)>;
using CloseCallback = std::function<void(TcpConnectionPtr)>;

class TcpConnection : public std::enable_shared_from_this<TcpConnection>
{
public:
  TcpConnection(EventLoop* loop,
                const std::string& name,
                int sockfd,
                const InetAddress& localAddr,
                const InetAddress& peerAddr);

  ~TcpConnection();

  EventLoop* getLoop() const { return loop_; }

  const std::string& name() const { return name_; }

  const InetAddress& localAddress() const { return localAddr_; }

  const InetAddress& peerAddress() const { return peerAddr_; }

  bool connected() const { return state_ == KConnected; }

  bool disconnected() const { return state_ == KDisconnected; }

  bool getTcpInfo(struct tcp_info*) const;

  std::string getTcpInfoString() const;

  void send(const void* message, int len);
  void send(Buffer* message);

  // 关闭连接操作
  void shutdown(); 
  void forceClose();
  void forceCloseWithDelay(double seconds);

  void setTcpNoDelay(bool on);


  void startRead();
  void stopRead();

  bool isReading() const { return reading_; }; 
  
  void setContext(const std::any& context) { context_ = std::move(context); }

  const std::any& getContext() const { return context_; }

  std::any* getMutableContext() { return &context_; }

  void setConnectionCallback(const ConnectionCallback& cb) { connectionCallback_ = cb; }

  void setMessageCallback(const MessageCallback& cb) { messageCallback_ = cb; }

  void setWriteCompleteCallback(const WriteCompleteCallback& cb) { writeCompleteCallback_ = cb; }

  void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark) { highWaterMarkCallback_ = cb; highWaterMark_ = highWaterMark; }

  Buffer* inputBuffer() { return &inputBuffer_; }

  Buffer* outputBuffer() { return &outputBuffer_; }

//   void setCloseCallback(const CloseCallback& cb)
//   { closeCallback_ = cb; }

  void connectEstablished(); //一旦connection 对象全都完成了 连接就绪时 就执行的接口
  void connectDestroyed();  //一旦connection 对象销毁时 就执行的接口

private:
  enum StateE { KDisconnected, KConnecting, KConnected, KDisconnecting };//连接状态

  void handleRead(base::Timestamp receiveTime);//设置channel 的 读事件回调
  void handleWrite();//设置channel 的 写事件回调
  void handleClose();//设置channel 的 关闭事件回调
  void handleError();//设置channel 的 错误事件回调
   
  //解决send线程安全问题，必须在所属的ioLoop中进行send
  void sendInLoop(const std::string& message);
  void sendInLoop(const void* message, size_t len);

  //解决跨线程关闭连接问题，必须在所属的ioLoop中进行关闭
  void shutdownInLoop();
  // void shutdownAndForceCloseInLoop(double seconds);
  void forceCloseInLoop();
  
  void setState(StateE s) { state_ = s; }
  const char* stateToString() const;
  void startReadInLoop();
  void stopReadInLoop();

  EventLoop* loop_;//所属的ioLoop
  const std::string name_;//标识符 连接的名称
  StateE state_;//连接状态
  bool reading_;//是否正在读取数据
 
  std::unique_ptr<Socket> socket_;//该连接对应的 socket 对象
  std::unique_ptr<Channel> channel_;//该连接对应的 channel 对象

  const InetAddress localAddr_;//本地地址信息
  const InetAddress peerAddr_;//对端地址信息


  ConnectionCallback connectionCallback_;//连接就绪时 + 连接销毁时 的回调函数 --- 使用者（用户）设置的
  MessageCallback messageCallback_;//消息到来时 or 有新消息时 的回调函数
  CloseCallback closeCallback_;//TcpServer设置的内部关闭回调函数 ，并不是使用者设置的 --- TcpServer内部设置的 --- 比如资源释放
  WriteCompleteCallback writeCompleteCallback_;//写操作完成时 的回调函数
  HighWaterMarkCallback highWaterMarkCallback_;//高水位标记回调函数 --- 对方发的特别快但我们服务器处理得很慢就会有数据积压在缓冲区里->当数据积压到一定数据后调用的回调函数


  size_t highWaterMark_;//高水位标记

  Buffer inputBuffer_;
  Buffer outputBuffer_; // FIXME: use list<Buffer> as output buffer.

  std::any context_;//上下文对象，常用于应用层协议处理
};

typedef std::shared_ptr<TcpConnection> TcpConnectionPtr;

}  // namespace net


