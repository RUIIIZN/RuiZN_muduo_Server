#pragma once

#include <vector>
#include <string>
#include <optional>

namespace net
{
    class Buffer
    {

    public:

        Buffer();
        Buffer(const Buffer& other);//复制构造函数
        Buffer(Buffer&& other);//移动构造函数
        ~Buffer();//析构函数

        Buffer& operator=(const Buffer& other);//赋值运算符
        Buffer& operator=(Buffer&& other);//移动赋值运算符

        void swap(Buffer& other);//交换两个Buffer区的内容

        const char* peek() const;//返回可读取位置的指针
        char* beginWrite();//返回写入起始位置的指针

        size_t readableBytes() const;//返回可读取字节数
        size_t writableBytes() const;//返回可写入字节数

        void retrieve(size_t len);//将读指针向后偏移 --- 吞掉字节
        void retrieveAll();//将读指针偏移到末尾 --- 写指针到哪就移动到哪
        std::string retrieveAllString();//获取所有数据通过string返回，并偏移读指针
        std::string retrieveAsString(size_t len);//获取指定长度。并以string返回

        void append(const char* data,size_t len);//可读数据末尾追加新数据

        void ensureWritableBytes(size_t len);//确保可写入字节数大于等于len --- 够了就返回，不够就扩容

        void hasWritten(size_t len);//将写指针向后偏移len字节

        ssize_t PrependableBytes();//获取前置空闲空间大小

        void prepend(void* data,size_t len);//可读数据开头追加新数据

        //c++17 新增的optional模板类，用于处理可能为空的值
        std::optional<std::string> getLine();//获取一行数据

        ssize_t readFd(int fd,int* savedErrno);//从fd读取数据到Buffer区中
        /*
            缓冲区剩余大小是固定的，fd能读的数据是不定的
            fd并不知道自己读取的数据量能否完全存在缓冲区中
            所以通过readv将放不下的数据存储在另一块不连续的临时空间中

            流程：
            1.优先读取剩余可读的字节大小到缓冲区
            2.如果缓冲区剩余空间不足，通过readv将放不下的数据存储在另一块不连续的临时空间中
            3.临时空间大小为64k（默认fd的缓冲区大小为64k）
        */



    private:
        char* begin();//获取空间起始地址
        const char* begin() const;//获取空间起始地址

        void makeSpace(size_t len);//buffer扩容

    private:

        const static int KInititalSize = 1024;//默认缓冲区大小
        const static int KCheapPrepend = 8;//预读头部大小方便应用层添加头部信息

        std::vector<char> buffer_;
        size_t readerIndex_;//读取位置索引
        size_t writerIndex_;//写入位置索引


    };
}