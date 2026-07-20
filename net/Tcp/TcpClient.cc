#include "TcpClient.h"

#include "../../base/log/Logging.h"
#include "TcpConnection.h"
#include "../eventloop/EventLoop.h"
#include "../socket/Socket.h"

#include <cerrno>
#include <functional>
#include <mutex>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>

using namespace net;

TcpClient::TcpClient(EventLoop* loop,
                     const InetAddress& serverAddr,
                     const std::string& nameArg)
  : baseloop_(loop),
    name_(nameArg),
    serverAddr_(serverAddr)
    {}

TcpClient::~TcpClient()
{
    if(connection_)
    {
        connection_->forceClose();
    }
}

void TcpClient::connect()
{
    int fd = sockets::createBlockingSocket();
    int res = sockets::connect(fd, serverAddr_.getSockAddr());
    int errn = res == 0 ? 0 : errno;
    switch (errn) 
    {
        case EISCONN://套接字已建立连接
        case EINPROGRESS://连接操作正在处理中 --- 非阻塞 --- 当描述符可写了才能代表连接成功
        case EINTR://被信号中断
        case 0:
        baseloop_->runInLoop(std::bind(&TcpClient::newConnection,this,fd));
        break;


        case EAGAIN://资源不足
        case EADDRINUSE://地址已使用
        case EADDRNOTAVAIL://地址不可用,通常在没有绑定地址时内核从空闲端口池中绑定地址但是端口池中无空闲端口
        case ECONNREFUSED://连接被拒绝,服务器没有对连接端口进行监听
        case ENETUNREACH://网络不可达 网络断开
        case ETIMEDOUT://超时
        retry(fd);
        break;

        case EPROTOTYPE://套接字类型错误
        case EACCES://权限错误
        case ENOTSOCK://不是套接字
        case EFAULT://地址无效
        case EBADF://文件描述符损坏
        case EALREADY://非阻塞套接字已经在连接服务器
        case EAFNOSUPPORT://地址族不支持该协议
        default:
        LOG_FATAL<<"连接服务器失败";
        break;
    }
}

TcpConnectionPtr TcpClient::connection()
{
    std::unique_lock<std::mutex> lock(mutex_);
    cond_.wait(lock,[this](){return (bool)connection_;});
    return connection_;
}

void TcpClient::retry(int sockfd)
{
    sockets::close(sockfd);
    baseloop_->runAfter(3,std::bind(&TcpClient::retryInLoop,this));
}

void TcpClient::retryInLoop()
{
    connect();
}


void TcpClient::newConnection(int sockfd)
{
  baseloop_->assertInLoopThread();
  InetAddress peerAddr(sockets::getPeerAddr(sockfd));
  char buf[32];
  snprintf(buf, sizeof buf, ":%s#%d", peerAddr.toIpPort().c_str(), sockfd);
  std::string connName = name_;

  InetAddress localAddr(sockets::getLocalAddr(sockfd));

  TcpConnectionPtr conn(new TcpConnection(baseloop_,
                                          connName,
                                          sockfd,
                                          localAddr,
                                          peerAddr));

  conn->setConnectionCallback(connectionCallback_);
  conn->setMessageCallback(messageCallback_);
  conn->setWriteCompleteCallback(writeCompleteCallback_);
  conn->setCloseCallback(std::bind(&TcpClient::removeConnection, this, std::placeholders::_1)); // FIXME: unsafe
  {
    std::lock_guard<std::mutex> lock(mutex_);
    connection_ = conn;
    cond_.notify_all();
  }
  conn->connectEstablished();
}

void TcpClient::removeConnection(const TcpConnectionPtr& conn)
{
    baseloop_->assertInLoopThread();
    {
        std::unique_lock<std::mutex> lock(mutex_);
        connection_.reset();
    }
    conn->connectDestroyed();
}