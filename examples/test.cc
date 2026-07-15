#include "../net/poller/Poller.h"
#include "../net/timer/TimerQueue.h"
#include "../base/log/Logging.h"
#include "../base/time/Timestamp.h"
#include "../net/eventloop/EventLoop.h"

#include <iostream>

// int main()
// {
//     net::EventLoop loop;
//     net::TimerQueue timer(&loop);
//     base::Timestamp now = base::Timestamp::now();

//     timer.addTimer([](){
//         std::cout<<"TimerQueue::handleRead()"<<std::endl;
//     },base::addTime(now, 13.0),0);

//     timer.addTimer([](){
//         std::cout<<"重复性2s定时任务"<<std::endl;
//     },base::addTime(now, 12.0),1);

//     auto tid = timer.addTimer([](){
//         std::cout<<"重复性4s定时任务"<<std::endl;
//     },base::addTime(now, 14.0),1);

//     timer.cancel(tid);

//     loop.loop();
//     return 0;
// }