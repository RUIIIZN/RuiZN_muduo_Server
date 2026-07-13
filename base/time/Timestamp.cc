#include "Timestamp.h"
#include <sys/time.h>

namespace net
{

Timestamp Timestamp::now()
{
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    int64_t seconds = tv.tv_sec;
    return Timestamp(seconds * kMicroSecondsPerSecond + tv.tv_usec);
}

}  // namespace net
