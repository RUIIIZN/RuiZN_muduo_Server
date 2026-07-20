#pragma once

#include "TcpConnection.h"

#include <atomic>
#include <cstdint>
#include <unordered_map>

namespace net
{

class Acceptor;
class EventLoop;
class EventLoopThreadPool;


class TcpServer
{
 public:
  typedef std::function<void(EventLoop*)> ThreadInitCallback;
  enum Option
  {
    kNoReusePort,
    kReusePort,
  };

  TcpServer(EventLoop* loop,
            const InetAddress& listenAddr,
            const std::string& nameArg,
            Option option = kNoReusePort);
  ~TcpServer();  

  const std::string& ipPort() const { return ipPort_; }
  const std::string& name() const { return name_; }
  EventLoop* getLoop() const { return loop_; }


  void setThreadNum(int numThreads);
  void setThreadInitCallback(const ThreadInitCallback& cb)
  { threadInitCallback_ = cb; }
  /// valid after calling start()
  std::shared_ptr<EventLoopThreadPool> threadPool()
  { return threadPool_; }

  void start();


  void setConnectionCallback(const ConnectionCallback& cb)
  { connectionCallback_ = cb; }


  void setMessageCallback(const MessageCallback& cb)
  { messageCallback_ = cb; }


  void setWriteCompleteCallback(const WriteCompleteCallback& cb)
  { writeCompleteCallback_ = cb; }

 private:

  void newConnection(int sockfd, const InetAddress& peerAddr);//新连接处理函数，设置给accept的回调函数 --- newConnectionCallback

  void removeConnection(const TcpConnectionPtr& conn);//从连接池中移除连接，用于conn连接释放
  void removeConnectionInLoop(const TcpConnectionPtr& conn);

  typedef std::unordered_map<std::string, TcpConnectionPtr> ConnectionMap;//连接管理池

  EventLoop* loop_; 
  const std::string ipPort_;
  const std::string name_;

  std::shared_ptr<Acceptor> acceptor_;
  std::shared_ptr<EventLoopThreadPool> threadPool_;

  ConnectionCallback connectionCallback_;
  MessageCallback messageCallback_;

  WriteCompleteCallback writeCompleteCallback_;

  ThreadInitCallback threadInitCallback_;

  std::atomic<bool> started_;

  std::atomic<int64_t> nextConnId_;
  ConnectionMap connections_;
};

}  // namespace net


