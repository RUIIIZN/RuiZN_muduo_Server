#include "../net/poller/Poller.h"
#include "../net/timer/TimerQueue.h"
#include "../base/log/Logging.h"
#include "../base/time/Timestamp.h"
#include "../net/eventloop/EventLoop.h"
#include "../net/timer/TimerId.h"
#include "../net/buffer/Buffer.h"
#include "../net/socket/Socket.h"
#include "../net/eventloop/EventLoopThreadPool.h"
#include "../net/acceptor/Acceptor.h"
#include "../net/Tcp/TcpConnection.h"

#include <iostream>



int main()
{
    return 0;
}

// void func1(){std::cout<<"2秒后定时任务"<<std::endl;}
// void func2(){std::cout<<"4秒后定时任务"<<std::endl;}
// void func3(){std::cout<<"6秒后循环定时任务"<<std::endl;}



// // 打印当前线程 ID 的辅助函数
// void printCurrentThread(const std::string& taskName)
// {
//     std::cout << "[Task: " << taskName 
//               << "] is running on Thread ID: " 
//               << std::this_thread::get_id() << std::endl;
// }

// 定义三个简单的测试任务
// void func1() { printCurrentThread("Func1 (RunAt)"); }
// void func2() { printCurrentThread("Func2 (RunAfter)"); }
// void func3() { printCurrentThread("Func3 (RunEvery)"); }

// int main()
// {
//     // 1. 打印主线程的 ID
//     std::cout << "======== Main Reactor (Thread ID: " 
//               << std::this_thread::get_id() << ") Started ========" << std::endl;

//     // 2. 初始化主 Reactor (MainLoop)
//     net::EventLoop loop;

//     // 3. 创建并启动线程池 (开启 2 个从 Reactor 子线程)
//     net::EventLoopThreadPool threadPool(&loop);
//     threadPool.setThreadNum(2);
//     threadPool.start(); // 此时子线程开始在底层安全创建各自的 EventLoop

//     // 4. 💡 核心步骤：获取两个从 Reactor (SubLoop) 的地址
//     // 轮询获取或者直接按索引获取
//     net::EventLoop* subLoop1 = threadPool.getNextLoop();
//     net::EventLoop* subLoop2 = threadPool.getNextLoop();

//     base::Timestamp now = base::Timestamp::now();

//     // 5. 💡 跨线程任务分发测试
    
//     // 任务一：在【主 Reactor】上运行定时任务 (2秒后)
//     loop.runAt(base::addTime(now, 2), &func1);

//     // 任务二：分发给【从 Reactor 1】运行定时任务 (4秒后)
//     // 注意：即使你在主线程里调用这个函数，runAfter 内部会安全地将任务跨线程投递到 subLoop1 的队列中
//     subLoop1->runAfter(4, &func2);

//     // 任务三：分发给【从 Reactor 2】运行周期性任务 (每 6 秒一次)
//     auto timerId = subLoop2->runEvery(6, &func3);

//     // 6. 开启主 Reactor 的事件循环
//     loop.loop();

//     return 0;
// }



// int main()
// {
//     net::EventLoop loop;
    
//     // 1. 启动线程池（多线程 Reactor 模型）
//     net::EventLoopThreadPool threadPool(&loop);
//     threadPool.setThreadNum(2);
//     threadPool.start();

//     // 2. 获取子线程的 loop 指针
//     net::EventLoop* subLoop1 = threadPool.getNextLoop();
//     net::EventLoop* subLoop2 = threadPool.getNextLoop();

//     base::Timestamp now = base::Timestamp::now();

//     // 3. 在不同的 Reactor 线程中分发注册定时任务（在 loop() 之前！）
//     loop.runAt(base::addTime(now, 2), std::move(func1));        // 主 Reactor
//     subLoop1->runAfter(4, std::move(func2));                    // 从 Reactor 1
//     auto timerId = subLoop2->runEvery(6, std::move(func3));     // 从 Reactor 2

//     // 4. 最后一步：启动主 Reactor 事件循环（开启引擎，进入死循环）
//     loop.loop(); 

//     return 0;
// }



// int main()
// {
//     net::EventLoop loop;
//     loop.loop();

//     base::Timestamp now = base::Timestamp::now();

//     loop.runAt(base::addTime(now,2),std::move(func1));
//     loop.runAfter(4,std::move(func2));
//     auto timerId = loop.runEvery(6,std::move(func3));
//     //loop.cancelTimer(timerId);

//     loop.loop();
//     return 0;
// }