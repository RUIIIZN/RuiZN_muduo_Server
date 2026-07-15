#pragma once

#include <cstdint>
#include <stdint.h>
#include <string>

namespace base
{

class Timestamp
{
public:
    Timestamp() : microSecondsSinceEpoch_(0) {}
    explicit Timestamp(int64_t microSecondsSinceEpoch)
        : microSecondsSinceEpoch_(microSecondsSinceEpoch)
    {}

    int64_t microSecondsSinceEpoch() const { return microSecondsSinceEpoch_; }

    static Timestamp now();//获取当前系统时间对应的时间戳对象

    static Timestamp invalid();//获取一个无效的时间戳对象
    bool valid();//判断时间戳是否合法

    std::string toFormatString();//关于日志输出

    //将unix中以秒为单位的时间戳转换为微秒为单位
    void fromUnixTime(time_t t);
    void fromUnixTime(time_t t,int usec);

    int64_t microSinceEpoch() const;
    int64_t secondSinceEpoch() const;

    static const int kMicroSecondsPerSecond = 1000 * 1000;

private:
    int64_t microSecondsSinceEpoch_;
};

inline bool operator<(Timestamp lhs, Timestamp rhs)
{
    return lhs.microSecondsSinceEpoch() < rhs.microSecondsSinceEpoch();
}

inline bool operator==(Timestamp lhs, Timestamp rhs)
{
    return lhs.microSecondsSinceEpoch() == rhs.microSecondsSinceEpoch();
}

inline Timestamp addTime(Timestamp timestamp, double seconds)
{
    int64_t delta = static_cast<int64_t>(seconds * Timestamp::kMicroSecondsPerSecond);
    return Timestamp(timestamp.microSecondsSinceEpoch() + delta);
}

}  // namespace net
