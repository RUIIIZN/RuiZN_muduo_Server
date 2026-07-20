#pragma once

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/uio.h>
#include "../inetaddress/InetAddress.h"


namespace net {
    namespace sockets 
    {
        static const int KListenCount = 1024;

        int createNonblockingSocket();
        int createBlockingSocket();

        int connect(int sockfd, const struct sockaddr *addr);
        void bind(int sockfd, const struct sockaddr *addr);
        void listen(int sockfd);
        int accept(int sockfd, struct sockaddr_in *addr);
        ssize_t read(int sockfd, void *buf, size_t count);
        ssize_t readv(int sockfd, const struct iovec *iov, int iovcnt);
        ssize_t write(int sockfd, const void *buf, size_t count);
        void close(int sockfd);
        //void shutdownWrite(int sockfd);

        void toIpPort(char *buf, size_t size, const struct sockaddr_in *addr);
        //void toIp(char *buf, size_t size, const struct sockaddr_in *addr);

        void fromIpPort(const char *ip, uint16_t port, struct sockaddr_in *addr);

        
        struct sockaddr_in getLocalAddr(int sockfd);
        struct sockaddr_in getPeerAddr(int sockfd);

    } // namespace sockets


    class Socket 
    {
        public:
        explicit Socket(int sockfd): sockfd_(sockfd) {}
        ~Socket() {sockets::close(sockfd_);}
        
        int fd() const { return sockfd_; }


        //bool getTcpInfo(struct tcp_info*) const;
        //bool getTcpInfoString(char* buf, int len) const;
        //void shutdownWrite();

        void bindAddress(const InetAddress& localaddr);
        void listen();
        int accept(InetAddress* peeraddr);


        void setTcpNoDelay(bool on);
        void setReuseAddr(bool on);
        void setReusePort(bool on);
        void setKeepAlive(bool on);

    private:
        const int sockfd_;
    };
} // namespace net
