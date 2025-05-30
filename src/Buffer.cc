#include <errno.h>
#include <sys/uio.h> // 提供 readv/writev 系统调用的定义
#include <unistd.h>  // 提供 POSIX API，如 read/write 函数等

#include "Buffer.h"

/**
 * 从fd上读取数据 Poller工作在LT模式
 * Buffer缓冲区是有大小的 但是从fd上读取数据的时候 却不知道tcp数据的最终大小
 * 
 * @description: 从socket读到缓冲区的方法是使用readv先读至buffer_，
 * buffer_空间如果不够，会读入到栈上65536个字节大小的空间，然后以append的方式追加如buffer_，
 * 既考虑了避免系统调用带来开销，又不影响数据的接收
 */


// 从fd上读取数据 fd-->buffer
ssize_t Buffer::readFd(int fd, int* saveErrno)
{
    // 栈额外空间 64KB
    char extrabuf[65536] = {0};// 分配64KB的栈空间数组作为额外缓冲区 当buffer_暂时不够用时，暂存数据

    /* iovec 结构体
    struct iovec {
        void* iov_base; // 指向的缓冲区的指针（存放的是readv接收的数据，或是writev将要发送的数据）
        size_t iov_len; // 缓冲区长度（读：最多能接收多少字节，写：实际要发送多少字节）
    }
    */   
    
    // 使用iovec分配两个连续的缓冲区
    struct iovec vec[2];
    const size_t writable = writableBytes(); // buffer当前可写空间大小

    // 第一块缓冲区，指向buffer可写空间
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;
    // 第二块缓冲区，指向备用占空间extrabuf
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof(extrabuf) ;

    // when there is enough space in this buffer, don't read into extrabuf
    // when extrabuf is used, we read 128KB - 1 bytes at most

    // 判断是否使用第二块缓冲区 extrabuf
    const int iovcnt = (writable < sizeof(extrabuf)) ? 2 : 1;

    // 执行系统调用 readv，从fd中读取数据 --> vec描述的缓冲区
    const ssize_t n = ::readv(fd, vec, iovcnt); // readv 会自动地按顺序写入多个缓冲区，返回读取字节数 n
    if (n < 0)              // 读取失败，保存错误码
    {
        *saveErrno = errno; 
    }
    else if (n <= writable) // 读取的数据量 < buffer可写空间，说明所有数据都写进buffer里的，直接更新指针
    {
        writerIndex_ += n;
    }
    else                    // n > writable 说明有部分数据写入了extrabuf 
    {
        writerIndex_ = buffer_.size();  // buffer_ 可写部分已写满
        append(extrabuf, n - writable); // 将extrabuf存储的另一部分追加到buffer_ (自动扩容)
    }

    // 返回读取的总字节数（含extrabuf中的数据）
    return n;
}


// inputBuffer_.readFd 表示将对端数据读到inputBuffer_中，移动writerIndex_指针
// outputBuffer_.writeFd 表示将数据写入到outputBuffer_中，从readerIndex_开始，可以写readableBytes()个字节

// 向fd写入数据   buffer-->fd
ssize_t Buffer::writeFd(int fd, int* saveErrno)
{
    ssize_t n = ::write(fd, peek(), readableBytes()); // 从 peek() 起始地址开始，向fd写入readableBytes()个字节
    if (n < 0)
    {
        *saveErrno = errno; // 写入失败，保存错误码
    }

    // 返回写入的字节数
    return n; 
}