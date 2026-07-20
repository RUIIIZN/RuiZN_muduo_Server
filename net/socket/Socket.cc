#include "Socket.h"
#include "../../base/log/Logging.h"

#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <linux/tcp.h>


namespace net 
{
    int sockets::createNonblockingSocket()
    {
        int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
        if (fd < 0)
        {
            LOG_SYSFATAL << "create NonblockingSocket failed";
        }
        return fd;
    }

    int sockets::createBlockingSocket()
    {
        int fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, IPPROTO_TCP);
        if (fd < 0)
        {
            LOG_SYSFATAL << "create BlockingSocket failed";
        }
        return fd;
    }

    void sockets::bind(int sockfd, const struct sockaddr *addr)
    {
        int ret = ::bind(sockfd, addr, sizeof(struct sockaddr_in));
        if (ret < 0)
        {
            LOG_SYSFATAL << "socket bind failed";
        }
    }

    int sockets::connect(int sockfd, const struct sockaddr *addr)
    {
        // int ret = ::connect(sockfd, addr, sizeof(struct sockaddr_in));
        // if (ret < 0)
        // {
        //     LOG_SYSERR << "socket connect server failed";
        // }
        // return ret;

        const struct sockaddr_in* ipv4 = reinterpret_cast<const struct sockaddr_in*>(addr);
        char ip[64];
        ::inet_ntop(AF_INET, &ipv4->sin_addr, ip, sizeof(ip));
        uint16_t port = ntohs(ipv4->sin_port);
        LOG_INFO << "即将连接到 IP: " << ip << ", Port: " << port;
        int ret = ::connect(sockfd, addr, sizeof(struct sockaddr_in));
        if (ret < 0)
        {
        int savedErrno = errno; // 💡 先把全局变量 errno 存起来，防止被后面的其他系统调用覆盖
        
        // 🟢 过滤掉非阻塞模式下的正常状态
        if (savedErrno == EINPROGRESS)
        {
            LOG_DEBUG << "Socket connect in progress (EINPROGRESS).";
        }
        else
        {
            // 🔴 真正的错误（如 ECONNREFUSED 等），通过 strerror 打印人类能看懂的信息
            LOG_SYSERR << "socket connect server failed: " 
                       << std::strerror(savedErrno) 
                       << " (errno=" << savedErrno << ")";
        }
        }
        return ret;
    }

    void sockets::listen(int sockfd)
    {
        int ret = ::listen(sockfd, KListenCount);
        if(ret < 0)
        {
            LOG_SYSERR << "socket listen failed";
        }
    }

    int sockets::accept(int sockfd, struct sockaddr_in *addr)
    {
        socklen_t len = sizeof(struct sockaddr_in);
        int res = accept4(sockfd, (struct sockaddr*)addr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if(res < 0)
        {
            switch (errno)
            {
                case EAGAIN://非阻塞场景下没有新连接
                     {LOG_TRACE << "非阻塞场景下没有新连接";break;}
                case ECONNABORTED://新连接异常
                     {LOG_TRACE << "新连接异常";break;}
                case EINTR://当前的阻塞操作被信号中断
                     {LOG_TRACE << "当前的阻塞操作被信号中断";break;}
                case EPROTO://协议错误
                     {LOG_TRACE << "协议错误";break;}
                case EPERM://防火墙拒绝
                     {LOG_TRACE << "防火墙拒绝";break;}
                case EMFILE://文件描述符数量超过进程最大限制
                     {LOG_TRACE << "文件描述符数量超过进程最大限制";break;}
                

                case EFAULT://文件描述符损坏
                     LOG_SYSFATAL << "文件描述符损坏";
                case EINVAL://地址参数无效
                     LOG_SYSFATAL << "地址参数无效";
                case ENFILE://系统文件描述符数量超过最大限制
                     LOG_SYSFATAL << "系统文件描述符数量超过最大限制";
                case ENOMEM://内存不足
                     LOG_SYSFATAL << "内存不足";
                case ENOTSOCK://文件描述符不是套接字
                     LOG_SYSFATAL << "文件描述符不是套接字";
                case EOPNOTSUPP://操作不支持: 描述符不是流式套接字
                     LOG_SYSFATAL << "操作不支持: 描述符不是流式套接字";
                case EBADF://文件描述符无效 
                     LOG_SYSFATAL << "文件描述符无效";


                default:
                    LOG_SYSFATAL << "socket accept failed";
                     break;
            }
        }
        return res;
    }

    ssize_t sockets::read(int sockfd, void *buf, size_t count)
    {
        return ::read(sockfd, buf, count);// == recv(sockfd, buf, count, 0)
    }
    ssize_t sockets::readv(int sockfd, const struct iovec *iov, int iovcnt)
    {
        return ::readv(sockfd, iov, iovcnt);//实现分块接收，将接收到的数据放到不连续的内存中
    }

    ssize_t sockets::write(int sockfd, const void *buf, size_t count)
    {
        return ::write(sockfd, buf, count);// == send(sockfd, buf, count, 0)
    }

    void sockets::close(int sockfd)
    {
        if(sockfd > 0)
        {
            ::close(sockfd);
        }
    }
    //void sockets::shutdownWrite(int sockfd);

    void sockets::toIpPort(char *buf, size_t size, const struct sockaddr_in *addr)
    {
        memset(buf, 0, size);
        inet_ntop(AF_INET, &addr->sin_addr, buf, size);
        snprintf(buf + strlen(buf),size - strlen(buf),"%d",ntohs(addr->sin_port));
    }

    void sockets::fromIpPort(const char *ip, uint16_t port, struct sockaddr_in *addr)
    {
        inet_pton(AF_INET, ip, &addr->sin_addr.s_addr);
        addr->sin_port = htons(port);
    }

    //void sockets::toIp(char *buf, size_t size, const struct sockaddr_in *addr);

    struct sockaddr_in sockets::getLocalAddr(int sockfd)
    {
        struct sockaddr_in localaddr;
        memset(&localaddr, 0, sizeof localaddr);
        socklen_t addrlen = static_cast<socklen_t>(sizeof localaddr);
        if (::getsockname(sockfd, (sockaddr*)(&localaddr), &addrlen) < 0)
        {
            LOG_SYSERR << "sockets::getLocalAddr";
        }
        return localaddr;   
    }

    struct sockaddr_in sockets::getPeerAddr(int sockfd)
    {
        struct sockaddr_in peeraddr;
        memset(&peeraddr, 0, sizeof peeraddr);
        socklen_t addrlen = static_cast<socklen_t>(sizeof peeraddr);
        if (::getpeername(sockfd, (sockaddr*)(&peeraddr), &addrlen) < 0)
        {
            LOG_SYSERR << "sockets::getPeerAddr";
        }
        return peeraddr;
    }




    void Socket::bindAddress(const InetAddress& localaddr)
    {
        sockets::bind(sockfd_, localaddr.getSockAddr());
    }

    void Socket::listen()
    {
        sockets::listen(sockfd_);
    }

    int Socket::accept(InetAddress* peeraddr)
    {
        struct sockaddr_in addr;
        int fd = sockets::accept(sockfd_, &addr);
        peeraddr->setSockAddr(addr);
        return fd;
    }


    void Socket::setTcpNoDelay(bool on)//设置TCP无延迟
    {
        int opt = on ? 1 : 0;
        setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
    }
    void Socket::setReuseAddr(bool on)//设置地址复用
    {
        int opt = on ? 1 : 0;
        setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    }
    void Socket::setReusePort(bool on)//设置端口复用
    {
        int opt = on ? 1 : 0;
        setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
    }
    void Socket::setKeepAlive(bool on)//设置保活
    {
        int opt = on ? 1 : 0;
        setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));
    }


}// namespace net
