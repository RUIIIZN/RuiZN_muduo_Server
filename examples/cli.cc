#include "../net/Tcp/TcpClient.h"
#include"../net/eventloop/EventLoopThread.h"

#include <iostream>

void onConnection(const net::TcpConnectionPtr& conn)
{
    if(conn->connected())
    {
        LOG_TRACE << conn->name() << " is connected" << "连接建立";
    }
    else 
    {
        LOG_TRACE << conn->name() << " is disconnected" << "连接断开";
    }
}


void onMessage(const net::TcpConnectionPtr& conn, net::Buffer* buf, base::Timestamp time)
{
    std::string res = buf->retrieveAllString();
    LOG_TRACE << conn->name() << " received : ";
    std::cout << res << std::endl;
    //conn->send(res.data(), res.size());
    //conn->forceClose();
}


int main()
{
    net::EventLoopThread loopThread;
    net::TcpClient client(loopThread.startLoop(),net::InetAddress("127.0.0.1",8888), "client");
    client.setConnectionCallback(onConnection);
    client.setMessageCallback(onMessage);
    client.connect();
    auto conn = client.connection();

    for(int i = 0; i < 10; i++)
    {
        std::string msg = "hello world " + std::to_string(i);
        conn->send(msg.data(), msg.size());
    }

}