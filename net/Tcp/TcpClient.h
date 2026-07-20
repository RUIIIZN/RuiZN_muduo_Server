#pragma once

#include "TcpConnection.h"
#include <mutex>
#include <condition_variable>

namespace net
{

class Connector;
typedef std::shared_ptr<Connector> ConnectorPtr;

class TcpClient
{
 public:
  TcpClient(EventLoop* loop,
            const InetAddress& serverAddr,
            const std::string& nameArg);
  ~TcpClient();

  void connect();// 创建套接字，连接服务器
  void disconnect();
  void stop();

  TcpConnectionPtr connection();// ==阻塞== 获取连接成功后构造的TcpConnection对象

  EventLoop* getLoop() const { return baseloop_; }

  const std::string& name() const
  { return name_; }

  void setConnectionCallback(ConnectionCallback cb)
  { connectionCallback_ = std::move(cb); }


  void setMessageCallback(MessageCallback cb)
  { messageCallback_ = std::move(cb); }

  void setWriteCompleteCallback(WriteCompleteCallback cb)
  { writeCompleteCallback_ = std::move(cb); }

 private:

  void retry(int sockfd);// 重试连接服务器
  void retryInLoop();// 重试连接服务器，确保在事件循环中调用

  void newConnection(int sockfd);// 新连接成功后调用，创建TcpConnection对象。唤醒connection的阻塞

  void removeConnection(const TcpConnectionPtr& conn);// 连接关闭后调用，移除成员管理 调用distroy

  EventLoop* baseloop_;
  const std::string name_;
  InetAddress serverAddr_;

  ConnectionCallback connectionCallback_;
  MessageCallback messageCallback_;
  WriteCompleteCallback writeCompleteCallback_;

  mutable std::mutex mutex_;
  TcpConnectionPtr connection_;// 连接成功后构造的TcpConnection对象
  std::condition_variable cond_;// 连接成功后唤醒
};

}  // namespace net