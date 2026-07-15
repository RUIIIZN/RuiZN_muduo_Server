#pragma once

#include <string>
#include <cstring>
#include <algorithm>

namespace base
{

const int kSmallBuffer = 4096;
const int kLargeBuffer = 4096 * 1024;

template<int SIZE>
class FixedBuffer
{
public:
    FixedBuffer() : cur_(data_) {}
    ~FixedBuffer() = default;

    void append(const char* buf, size_t len)
    {
        if (static_cast<size_t>(avail()) > len)
        {
            memcpy(cur_, buf, len);
            cur_ += len;
        }
    }

    const char* data() const { return data_; }
    int length() const { return static_cast<int>(cur_ - data_); }
    char* current() { return cur_; }
    int avail() const { return static_cast<int>(end() - cur_); }
    void add(size_t len) { cur_ += len; }
    void reset() { cur_ = data_; }
    void bzero() { memset(data_, 0, sizeof(data_)); }

private:
    const char* end() const { return data_ + sizeof(data_); }

    char data_[SIZE];
    char* cur_;
};

class LogStream
{
    using self = LogStream;
public:
    using Buffer = FixedBuffer<kSmallBuffer>;

    self& operator<<(bool v)
    {
        buffer_.append(v ? "1" : "0", 1);
        return *this;
    }

    self& operator<<(short);
    self& operator<<(unsigned short);
    self& operator<<(int);
    self& operator<<(unsigned int);
    self& operator<<(long);
    self& operator<<(unsigned long);
    self& operator<<(long long);
    self& operator<<(unsigned long long);

    self& operator<<(const void*);

    self& operator<<(float v)
    {
        *this << static_cast<double>(v);
        return *this;
    }
    self& operator<<(double);

    self& operator<<(char v)
    {
        buffer_.append(&v, 1);
        return *this;
    }

    self& operator<<(const char* str)
    {
        if (str)
            buffer_.append(str, strlen(str));
        else
            buffer_.append("(null)", 6);
        return *this;
    }

    self& operator<<(const unsigned char* str)
    {
        return operator<<(reinterpret_cast<const char*>(str));
    }

    self& operator<<(const std::string& str)
    {
        buffer_.append(str.data(), str.size());
        return *this;
    }

    void append(const char* data, int len) { buffer_.append(data, len); }
    const Buffer& buffer() const { return buffer_; }
    void resetBuffer() { buffer_.reset(); }

private:
    template<typename T>
    void formatInteger(T);

    Buffer buffer_;

    static const int kMaxNumericSize = 32;
};

template<typename T>
void LogStream::formatInteger(T v)
{
    if (buffer_.avail() >= kMaxNumericSize)
    {
        char* buf = buffer_.current();
        char* start = buf;
        bool negative = false;

        if (v < 0)
        {
            negative = true;
            v = -v;
        }

        do
        {
            *buf++ = static_cast<char>(v % 10) + '0';
            v /= 10;
        } while (v > 0);

        if (negative)
            *buf++ = '-';

        *buf = '\0';
        std::reverse(start, buf);
        buffer_.add(static_cast<int>(buf - start));
    }
}

}  // namespace muduo