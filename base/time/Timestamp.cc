#include "Timestamp.h"
#include <sys/time.h>
#include <ctime>
#include "../../base/log/Logging.h"
namespace base
{

Timestamp Timestamp::now()
{
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    int64_t seconds = tv.tv_sec;

    LOG_INFO << "Timestamp now" << seconds << " " << tv.tv_usec;

    return Timestamp(seconds * kMicroSecondsPerSecond + tv.tv_usec);
}

Timestamp Timestamp::invalid()
{
    return Timestamp(0);
}

bool Timestamp::valid()//判断时间戳是否合法
{
    return microSecondsSinceEpoch_ > 0; 
}
std::string Timestamp::toFormatString()//关于日志输出
{
    time_t t = microSecondsSinceEpoch_ / kMicroSecondsPerSecond;
    struct tm lt;
    localtime_r(&t, &lt);
    char buf[32];
    snprintf(buf, sizeof(buf), "%4d-%02d-%02d %02d:%02d:%02d",
             lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday,
             lt.tm_hour, lt.tm_min, lt.tm_sec);
    return buf;
}

//将unix中以秒为单位的时间戳转换为微秒为单位
void Timestamp::fromUnixTime(time_t t)
{
    return fromUnixTime(t,0);
}
void Timestamp::fromUnixTime(time_t t,int usec)
{
    microSecondsSinceEpoch_ = t * kMicroSecondsPerSecond + usec;
}

int64_t Timestamp::microSinceEpoch() const
{
    return microSecondsSinceEpoch_;
}
int64_t Timestamp::secondSinceEpoch() const
{
    return microSecondsSinceEpoch_ / kMicroSecondsPerSecond;
}

}  // namespace net