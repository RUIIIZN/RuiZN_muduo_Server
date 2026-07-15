#pragma once

#include "../../base/time/Timestamp.h"
#include <functional>
#include "TimerId.h"
#include <atomic>

typedef std::function<void()> TimerCallback;

using base::Timestamp;
namespace net
{
    class Timer//定时任务类：超时了之后要干什么：什么时候超时，是否是重复性任务
    {
        public:

            Timer(TimerCallback cb,Timestamp when,double interval)
            :callback_(std::move(cb))
            ,expiration_(when)
            ,interval_(interval)
            ,repeat_(interval > 0.0)
            ,sequence_(s_numCreated_.fetch_add(1))
            {}

            Timer(Timer&& other) = default;

            Timer& operator=(Timer&& other) = default;

            void run() { callback_(); }//执行定时任务要执行的操作

            Timestamp expiration() const { return expiration_; }//获取过期时间

            int64_t getsequence() const { return sequence_; }//获取唯一身份标识

            bool repeated() const { return repeat_; }//是否是重复性任务

            double  interval() const { return interval_; }//获取定时任务重复间隔

            void restart(Timestamp now);//根据当前系统时间重新启动定时任务

        private:
            TimerCallback callback_;//定时任务要执行的操作

            Timestamp expiration_;//定时任务超时时间

            double interval_;//定时任务重复间隔

            bool repeat_;//是否是重复性任务

            int64_t sequence_;//唯一身份标识

            static std::atomic<int64_t> s_numCreated_;//递增序号
    };

}