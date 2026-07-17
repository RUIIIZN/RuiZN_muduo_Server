#pragma once

#include <string>
#include <netinet/in.h>


namespace net
{
class InetAddress
{
    public:
        explicit InetAddress(uint16_t port = 0);

        InetAddress(const std::string ip, uint16_t port);//将字符串转换为sockaddr_in结构体

        std::string toIpPort() const;//讲网络地址结构转换为字符串

        const struct sockaddr * getSockAddr() const;//获取sockaddr结构体指针

        void setSockAddr(const struct sockaddr_in& addr);//设置地址数据

    private:
        struct sockaddr_in addr_;
};
}  // namespace net