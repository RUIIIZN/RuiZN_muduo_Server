#include "Acceptor.h"

#include <assert.h>

namespace net
{
   Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport):
      loop_(loop),
      acceptSocket_(sockets::createNonblockingSocket()),
      acceptChannel_(loop, acceptSocket_.fd()),
      listening_(false),
      idleFd_(::open("dev/null", O_CLOEXEC | O_CREAT))
      {
        assert(idleFd_ >= 0);
        LOG_TRACE << "Acceptor::Acceptor() idleFd_ = " << idleFd_;

        acceptSocket_.setTcpNoDelay(true);
        LOG_TRACE << "Acceptor::Acceptor() setTcpNoDelay ok";

        acceptSocket_.setReuseAddr(true);
        LOG_TRACE << "Acceptor::Acceptor() setReuseAddr ok";

        acceptSocket_.setReusePort(reuseport);
        LOG_TRACE << "Acceptor::Acceptor() setReusePort ok";

        acceptSocket_.bindAddress(listenAddr);
        LOG_TRACE << "Acceptor::Acceptor() bindAddress ok";

        acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
        LOG_TRACE << "Acceptor::Acceptor() setReadCallback ok";
      }

   Acceptor::~Acceptor()
   { 
    ::close(idleFd_);
    acceptChannel_.disableAll();
    acceptChannel_.remove();
   }

   void Acceptor::listen()
   {
    loop_->assertInLoopThread();
    listening_ = true;
    acceptSocket_.listen();
    acceptChannel_.enableReading();
   }

   void Acceptor::handleRead()
   {
    loop_->assertInLoopThread();

    InetAddress peerAddr;//用于获取对端地址信息
    int connfd = acceptSocket_.accept(&peerAddr);


    if (connfd >= 0)
    {
     // string hostport = peerAddr.toIpPort();
     // LOG_TRACE << "Accepts of " << hostport;
     if (newConnectionCallback_)
     {
       newConnectionCallback_(connfd, peerAddr);
     }
     else
     {
       sockets::close(connfd);
     }
    }
    else
    {
      LOG_SYSERR << "in Acceptor::handleRead";

      if (errno == EMFILE)//用于应对文件描述符耗尽问题
      {
        ::close(idleFd_);
        idleFd_ = ::accept(acceptSocket_.fd(), NULL, NULL);
        ::close(idleFd_);
        idleFd_ = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
      }

    }
   }

}// namespace net