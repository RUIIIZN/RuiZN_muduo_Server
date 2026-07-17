#include "Buffer.h"
#include "../socket/Socket.h"
#include "../../base/log/Logging.h"

#include <utility>
#include <assert.h>
#include <cstring>
#include <sys/uio.h>

namespace net
{
    Buffer::Buffer(): 
        buffer_(KInititalSize), 
        readerIndex_(KCheapPrepend), 
        writerIndex_(KCheapPrepend) {}

    Buffer::Buffer(const Buffer& other):
        buffer_(other.buffer_), 
        readerIndex_(other.readerIndex_), 
        writerIndex_(other.writerIndex_){}

    Buffer::Buffer(Buffer&& other)
    {
        Buffer tmp;
        tmp.swap(other);
        tmp.swap(*this);
    }
        
    Buffer::~Buffer(){}

    Buffer& Buffer::operator=(const Buffer& other)//赋值运算符
    {
        Buffer tmp(other);
        tmp.swap(*this);
        return *this;
    }
    Buffer& Buffer::operator=(Buffer&& other)//移动赋值运算符
    {
        Buffer tmp(std::move(other));
        tmp.swap(*this);
        return *this;
    }

    void Buffer::swap(Buffer& other)//交换两个Buffer区的内容
    {
        std::swap(readerIndex_,other.readerIndex_);
        std::swap(writerIndex_,other.writerIndex_);
        buffer_.swap(other.buffer_);
    }

    char* Buffer::begin()//获取空间起始地址
    {
        return &*buffer_.begin();
    }
    const char* Buffer::begin() const//获取空间起始地址
    {
        return &*buffer_.begin();
    }

    const char* Buffer::peek() const//返回可读取位置的指针
    {
        return begin() + readerIndex_;
    }

    char* Buffer::beginWrite()//返回写入起始位置的指针
    {
        return begin() + writerIndex_; 
    }

    size_t Buffer::readableBytes() const//返回可读取字节数
    {
        return writerIndex_ - readerIndex_;
    }
    size_t Buffer::writableBytes() const//返回可写入字节数
    {
        return buffer_.size() - writerIndex_;
    }

    void Buffer::retrieve(size_t len)//将读指针向后偏移 --- 吞掉字节
    {
       assert(len <= readableBytes());
       if (len < readableBytes())
       {
          readerIndex_ += len;
       }
       else
       {
          retrieveAll();
       }
    }

    void Buffer::retrieveAll()//将读指针偏移到末尾 --- 写指针到哪就移动到哪
    {
        readerIndex_ = Buffer::KCheapPrepend;
        writerIndex_ = Buffer::KCheapPrepend;
    }

    std::string Buffer::retrieveAsString(size_t len)////获取指定长度。并以string返回
    {
        assert(len <= readableBytes());
        std::string result(peek(), len);
        retrieve(len);
        return result;
    }

     std::string Buffer::retrieveAllString()//获取所有数据通过string返回，并偏移读指针
    {
        return retrieveAsString(Buffer::readableBytes());
    }

    void Buffer::append(const char* data,size_t len)//可读数据末尾追加新数据
    {
        ensureWritableBytes(len);
        std::copy(data, data+len, beginWrite());
        hasWritten(len);
    }

    void Buffer::ensureWritableBytes(size_t len)//确保可写入字节数大于等于len --- 够了就返回，不够就扩容
    {
        if (Buffer::writableBytes() < len)
        {
            makeSpace(len);
        }
        assert(Buffer::writableBytes() >= len);
    }

    void Buffer::hasWritten(size_t len)//将写指针向后偏移len字节
    {
        assert(len <= Buffer::writableBytes());
        writerIndex_ += len;
    }

    ssize_t Buffer::PrependableBytes()//获取前置空闲空间大小
    {
        return readerIndex_;
    }

    void Buffer::makeSpace(size_t len)//buffer扩容
    {
        //1.前置空闲空间+后置空闲空间 < len + KCheapPrepend  -> 向后扩容
        //2.后置空闲空间不足，但是 + 前置空闲空间足够了  -> 向前拷贝到起始位置 -> 从而使后置空闲空间足够
        if(PrependableBytes() + writableBytes() < len + KCheapPrepend)
        {
            buffer_.resize(writerIndex_ + len);
        }
        else
        {
            assert(KCheapPrepend < readerIndex_);
            size_t datasize = readableBytes();
            std::copy(begin()+readerIndex_,begin()+writerIndex_,begin()+KCheapPrepend);
            readerIndex_ = KCheapPrepend;
            writerIndex_ = readerIndex_ + datasize;
            assert(datasize == readableBytes());
        }
    }

    void Buffer::prepend(void* data,size_t len)//可读数据开头追加新数据 --- 针对前置的小空间（KCheapPrepend大小）中添加
    {
        assert(len <= PrependableBytes());
        readerIndex_ -= len;
        const char* d = static_cast<const char*>(data);
        std::copy(d, d+len, begin()+readerIndex_);
    }

    std::optional<std::string> Buffer::getLine()//获取一行数据
    {
        void* pos = memchr((void*)peek(),'\n',readableBytes());
        if(pos == nullptr)
        {
            return std::nullopt;
        }
        std::string res(peek(),(char*)pos - peek() + 1);
        retrieve(res.size());
        return res;
    }

    ssize_t Buffer::readFd(int fd,int* savedErrno)//从fd读取数据到Buffer区中 --- muduo的pingpong测试高出libevent(fd一次最大读取4096字节)的性能1倍多 --- 为什么用水平触发而不用边缘触发
    {
        char extrabuf[65536];//64k
        struct iovec vec[2];
        const size_t writable = writableBytes();   
        vec[0].iov_base = begin()+writerIndex_;
        vec[0].iov_len = writable;
        vec[1].iov_base = extrabuf;
        vec[1].iov_len = sizeof extrabuf;

        const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;//是否需要额外空间

        const ssize_t res = sockets::readv(fd, vec, iovcnt);

        if (res < 0)//通过返回值判断extrabuf中是否有数据
        {
            *savedErrno = errno;
            LOG_SYSERR << "readv failed : res < 0";
        }
        else if (res <= writable)//第二块空间（extrabuf）没用上
        {
            writerIndex_ += res;
        }
        else//extrabuf有数据
        {
            writerIndex_ = buffer_.size();//将写指针向后偏移到末尾---buffer填满了
            append(extrabuf,res - writable);//将extrabuf中剩余数据追加到buffer中
        }
        // if (n == writable + sizeof extrabuf)
        // {
        //   goto line_181;
        // }
        return res;
    }


}//namespace net