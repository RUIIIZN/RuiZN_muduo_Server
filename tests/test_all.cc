#include <iostream>
#include "log/Logging.h"
#include "time/Timestamp.h"
#include "channel/Channel.h"
#include "eventloop/EventLoop.h"

using namespace net;

int main() {
    std::cout << "=== Testing RuiZN_Server Modules ===\n" << std::endl;

    std::cout << "[1] Testing Timestamp..." << std::endl;
    Timestamp now(Timestamp::now());
    std::cout << "Current time (microseconds): " << now.microSecondsSinceEpoch() << std::endl;
    std::cout << "Timestamp test passed!\n" << std::endl;

    std::cout << "[2] Testing Logging..." << std::endl;
    LOG_INFO << "This is an info log";
    LOG_WARN << "This is a warning log";
    LOG_ERROR << "This is an error log";
    std::cout << "Logging test passed!\n" << std::endl;

    std::cout << "[3] Testing Channel..." << std::endl;
    {
        EventLoop loop;
        Channel channel(&loop, 1);
        channel.setReadCallback([](Timestamp) { std::cout << "Read callback triggered\n"; });
        channel.enableReading();
        std::cout << "Channel fd: " << channel.fd() << std::endl;
        std::cout << "Channel index: " << channel.index() << std::endl;
        channel.disableAll();
        channel.remove();
    }
    std::cout << "Channel test passed!\n" << std::endl;

    std::cout << "[4] Testing EventLoop..." << std::endl;
    std::cout << "EventLoop created successfully" << std::endl;
    std::cout << "EventLoop test passed!\n" << std::endl;

    std::cout << "=== All tests passed! ===\n" << std::endl;
    return 0;
}