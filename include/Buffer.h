#pragma once

#include <vector>
#include <string>
#include <algorithm>
#include <stddef.h>  // 定义size_t类型

/* 网络库底层的缓冲区类型定义 */

class Buffer
{
public:
    // 静态常量：缓冲区预留区和初始大小
    static const size_t kCheapPrepend = 8;  // 预留的前置空间大小 8B
    static const size_t kInitialSize = 1024;// 默认缓冲区初始大小 1KB

    // 构造 
    explicit Buffer(size_t initalSize = kInitialSize) // 传入缓冲区初始大小 默认为kInitialSize(1024B)
        : buffer_(kCheapPrepend + initalSize) // 分配缓冲区大小 = prepend预留 + initialSize
        , readerIndex_(kCheapPrepend)         // 读指针初始设置在有效数据的起始位置
        , writerIndex_(kCheapPrepend)         // 写指针也设置在起始位置，表示没有数据可读
    {
        // 初始化时分配(kCheapPrepend + initalSize)的空间，读写位置都从kCheapPrepend开始
    }


    // |<--- prependableBytes() --->|<--- readableBytes() --->|<---- writableBytes() ---->|
    // |         8 bytes           |                         |                            |
    // |---------------------------|-------------------------|----------------------------|
    // ^                           ^                         ^                            ^
    // buffer_.begin()          readerIndex_           writerIndex_              buffer_.end()
    

    // 可读字节 = 已写入数据长度
    size_t readableBytes() const { return writerIndex_ - readerIndex_; }   
    // 可写字节 = buffer总长 - 写指针 
    size_t writableBytes() const { return buffer_.size() - writerIndex_; } 
    // prepend前置区间 = 读指针左边的空间 
    size_t prependableBytes() const { return readerIndex_; }                

    // 获取当前可读数据的起始地址
    const char* peek() const { return begin() + readerIndex_; }
    
    // 从缓冲区中读取len长度数据（仅移动readerIndex_，不拷贝数据）
    void retrieve(size_t len)
    {
        // 请求读取的数据 len 小于当前的可读数据 readableBytes()
        if (len < readableBytes()) 
        {
            readerIndex_ += len; // 前移 readerIndex_ 指针，表示前 len 字节已经被读取
        }
        else // len >= readableBytes()  全部可读数据已读取
        {
            retrieveAll(); // 调用清空操作 重置读写指针
        }
    }

    // 重置读写位置，清空缓冲区
    void retrieveAll() 
    {
        readerIndex_ = kCheapPrepend;
        writerIndex_ = kCheapPrepend;
    }

    // 以string形式返回所有可读数据，并清空缓冲区
    std::string retrieveAllAsString() { return retrieveAsString(readableBytes()); } // retrieveAsString(所有可读)

    // 以string形式返回指定长度的数据，并移动readerIndex_
    std::string retrieveAsString(size_t len)
    {
        std::string result(peek(), len); // 从peek处构造指定长度len的字符串（拷贝出来）
        retrieve(len); // 向前移动readerIndex_
        return result;
    }


    // 保证缓冲区至少有len字节的可写空间
    void ensureWritableBytes(size_t len)
    {
        if (writableBytes() < len)
        {
            makeSpace(len); // 否则扩容 buffer_.size() - writerIndex_
        }
    }

    // 把外部内存[data, data+len]上的数据添加到writable缓冲区末尾（自动扩容）
    void append(const char* data, size_t len)
    {
        // data - 指向要写入数据的起始地址   len - 要写入的长度（字节）
        ensureWritableBytes(len); // 确保有足够len空间
        std::copy(data, data+len, beginWrite()); // copy逐字节复制数据 写入可写位置
        writerIndex_ += len; // 更新writerIndex_
    }

    // 获取当前可写入位置的起始地址 
    char* beginWrite() { return begin() + writerIndex_; }
    const char* beginWrite() const { return begin() + writerIndex_; } // const重载

    // 从fd上读取数据 fd-->buffer
    ssize_t readFd(int fd, int* saveErrno);
    // 向fd写入数据   buffer-->fd
    ssize_t writeFd(int fd, int* saveErrno);


private:
    // 获取buffer_底层数组的起始地址
    char* begin() { return &*buffer_.begin(); } // vector底层数组首元素的地址 
    const char* begin() const { return &*buffer_.begin(); } // const重载（为了const buffer也能调用）
    
    // 扩容 确保至少有len字节可写
    void makeSpace(size_t len)
    {
        /**
         * | kCheapPrepend |xxx| reader | writer |                     // xxx标示reader中已读的部分
         * | kCheapPrepend | reader ｜          len          |
         **/
        
        // 若可写区+前置区不足以满足要求，直接扩容
        if (writableBytes() + prependableBytes() < len + kCheapPrepend) // 即len > 已读 + writer的部分
        {
            buffer_.resize(writerIndex_ + len); // 扩大缓冲区容量
        }
        else // len <= 已读 + writer  空间够 只需将已有未读数据前移到kCheapPrepend
        {
            size_t readable = readableBytes(); // readable = reader的长度

            // 将readerIndex_到writerIndex_的数据 => 拷贝到起始kCheapPrepend处，腾出更多的可写空间 
            std::copy(begin() + readerIndex_,   
                      begin() + writerIndex_,   // [begin() + readerIndex_, begin() + writerIndex_)]
                      begin() + kCheapPrepend); // 移动到kCheapPrepend
            
            // 调整可读可写指针
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readable;
        }
    }

    std::vector<char> buffer_; // 实际缓冲区
    size_t readerIndex_; // 可读的起始位置
    size_t writerIndex_; // 可写的起始位置
    
};

