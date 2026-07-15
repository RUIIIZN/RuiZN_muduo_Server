#pragma once

#include "LogStream.h"
#include <string>

namespace base
{

class Logger
{
public:
    enum LogLevel
    {
        TRACE,
        DEBUG,
        INFO,
        WARN,
        ERROR,
        FATAL,
        NUM_LOG_LEVELS,
    };

    class SourceFile
    {
    public:
        template<int N>
        SourceFile(const char (&arr)[N])
            : data_(arr), size_(N-1)
        {
            const char* slash = strrchr(data_, '/');
            if (slash)
            {
                data_ = slash + 1;
                size_ -= static_cast<int>(data_ - arr);
            }
        }

        explicit SourceFile(const char* filename)
            : data_(filename)
        {
            const char* slash = strrchr(data_, '/');
            if (slash)
            {
                data_ = slash + 1;
            }
            size_ = static_cast<int>(strlen(data_));
        }

        const char* data_;
        int size_;
    };

    Logger(SourceFile file, int line);
    Logger(SourceFile file, int line, LogLevel level);
    Logger(SourceFile file, int line, LogLevel level, const char* func);
    Logger(SourceFile file, int line, bool toAbort);
    ~Logger();

    LogStream& stream() { return impl_.stream_; }

    static LogLevel logLevel();
    static void setLogLevel(LogLevel level);

    using OutputFunc = void(*)(const char* msg, int len);
    using FlushFunc = void(*)(void);
    static void setOutput(OutputFunc);
    static void setFlush(FlushFunc);

private:

    class Impl
    {
    public:
        using LogLevel = Logger::LogLevel;
        Impl(LogLevel level, int old_errno, const SourceFile& file, int line);
        void formatTime();
        void finish();

        LogStream stream_;
        LogLevel level_;
        int line_;
        SourceFile basename_;
    };

    Impl impl_;
};

extern Logger::LogLevel g_logLevel;

inline Logger::LogLevel Logger::logLevel()
{
    return g_logLevel;
}

#define LOG_TRACE if (base::Logger::logLevel() <= base::Logger::TRACE) \
    base::Logger(__FILE__, __LINE__, base::Logger::TRACE, __func__).stream()
#define LOG_DEBUG if (base::Logger::logLevel() <= base::Logger::DEBUG) \
    base::Logger(__FILE__, __LINE__, base::Logger::DEBUG, __func__).stream()
#define LOG_INFO if (base::Logger::logLevel() <= base::Logger::INFO) \
    base::Logger(__FILE__, __LINE__).stream()
#define LOG_WARN base::Logger(__FILE__, __LINE__, base::Logger::WARN).stream()
#define LOG_ERROR base::Logger(__FILE__, __LINE__, base::Logger::ERROR).stream()
#define LOG_FATAL base::Logger(__FILE__, __LINE__, base::Logger::FATAL).stream()

#define LOG_SYSERR base::Logger(__FILE__, __LINE__, false).stream()
#define LOG_SYSFATAL base::Logger(__FILE__, __LINE__, true).stream()

}  // namespace base