#include "InetAddress.h"
#include "../socket/Socket.h"

namespace net
{
InetAddress::InetAddress(uint16_t port)
{
    addr_.sin_addr.s_addr = INADDR_ANY;
    addr_.sin_port = htons(port);
    addr_.sin_family = AF_INET;
}

InetAddress::InetAddress(const std::string ip, uint16_t port)//将字符串转换为sockaddr_in结构体
{
    addr_.sin_family = AF_INET;
    sockets::fromIpPort(ip.c_str(),port,&addr_);
}

std::string InetAddress::toIpPort() const//讲网络地址结构转换为字符串
{
    char buf[64];
    sockets::toIpPort(buf, 64, &addr_);
    return buf;
}

const struct sockaddr * InetAddress::getSockAddr() const//获取sockaddr 结构体指针
{
    return (const struct sockaddr*)&addr_;
}

void InetAddress::setSockAddr(const struct sockaddr_in& addr)//设置地址数据
{
    addr_ = addr;
}

}  // namespace net