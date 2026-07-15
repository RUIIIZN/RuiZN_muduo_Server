#include "Logger.h"
#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

namespace base
{

Logger::LogLevel g_logLevel = Logger::TRACE;

const char* LogLevelName[Logger::NUM_LOG_LEVELS] =
{
    "TRACE ",
    "DEBUG ",
    "INFO  ",
    "WARN  ",
    "ERROR ",
    "FATAL ",
};

Logger::OutputFunc g_output = [](const char* msg, int len) {
    fwrite(msg, 1, len, stdout);
};

Logger::FlushFunc g_flush = []() {
    fflush(stdout);
};

Logger::Impl::Impl(LogLevel level, int savedErrno, const SourceFile& file, int line)
    : stream_(),
      level_(level),
      line_(line),
      basename_(file)
{
    formatTime();
    stream_ << LogLevelName[level];
    if (savedErrno != 0)
    {
        stream_ << strerror(savedErrno) << " (errno=" << savedErrno << ") ";
    }
}

void Logger::Impl::formatTime()
{
    struct timeval tv;
    time_t time;
    char strTime[26] = {0};
    gettimeofday(&tv, NULL);
    time = tv.tv_sec;
    struct tm* pTime = localtime(&time);
    strftime(strTime, 26, "%Y-%m-%d %H:%M:%S.", pTime);
    stream_ << strTime;
    char usec[7] = {0};
    snprintf(usec, 7, "%06d", static_cast<int>(tv.tv_usec));
    stream_ << usec;
}

void Logger::Impl::finish()
{
    stream_ << " - " << basename_.data_ << ':' << line_ << '\n';
}

Logger::Logger(SourceFile file, int line)
    : impl_(INFO, 0, file, line)
{
}

Logger::Logger(SourceFile file, int line, LogLevel level)
    : impl_(level, 0, file, line)
{
}

Logger::Logger(SourceFile file, int line, LogLevel level, const char* func)
    : impl_(level, 0, file, line)
{
    impl_.stream_ << func << ' ';
}

Logger::Logger(SourceFile file, int line, bool toAbort)
    : impl_(toAbort ? FATAL : ERROR, errno, file, line)
{
}

Logger::~Logger()
{
    impl_.finish();
    const LogStream::Buffer& buf(stream().buffer());
    g_output(buf.data(), buf.length());
    if (impl_.level_ == FATAL)
    {
        g_flush();
        abort();
    }
}

void Logger::setOutput(OutputFunc out)
{
    g_output = out;
}

void Logger::setFlush(FlushFunc flush)
{
    g_flush = flush;
}

}  // namespace muduo