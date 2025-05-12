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
    // 额外栈空间 64KB
    char extrabuf[65536] = {0}; // 分配64KB的栈空间数组作为额外缓冲区 当buffer_暂时不够用时，暂存数据

    /*
    struct iovec {
        ptr_t iov_base; // iov_base指向的缓冲区 存放的是readv接收的数据，或是writev将要发送的数据
        size_t iov_len; // iov_len在各种情况下分别确定了接收的最大长度以及实际写入的长度
    }
    */

    // 使用iovec分配两个连续的缓冲区
    struct iovec vec[2];
    const size_t writable = writableBytes(); // buffer当前可写空间大小

    // 第一块缓冲区，指向buffer可写空间
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;
    // 第二块缓冲区，指向备用栈空间 extrabuf，用于扩展容量
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof(extrabuf);

    // when there is enough space in this buffer, don't read into extrabuf
    // when extrabuf is used, we read 128k-1 bytes at most
    // 这里之所以说最多128k-1字节，是因为若writable为64k-1，那么需要两个缓冲区 第一个64k-1 第二个64k 所以最多128k-1
    // 如果第一个缓冲区>=64k 那就只采用一个缓冲区 而不使用栈空间extrabuf[65536]的内容
    
    // 判断是否使用第二个缓冲区 extrabuf
    const int iovcnt = (writable < sizeof(extrabuf)) ? 2 : 1;

    // 执行系统调用 readv，从fd中读取数据 --> vec描述的缓冲区
    const ssize_t n = ::readv(fd, vec, iovcnt); // readv 会自动地按顺序写入多个缓冲区，返回读取字节数 n
    if (n < 0) // 读取失败，保存错误码
    {
        *saveErrno = errno; 
    }
    else if (n <= writable) // buffer_可写缓冲区足够存储读出来的数据，直接更新指针
    {
        writerIndex_ += n; 
    }
    else // // buffer_ 空间不够，部分数据写入了 extrabuf
    {
        writerIndex_ = buffer_.size();  // buffer_ 可写部分已用满
        append(extrabuf, n - writable); // 对buffer_扩容，并将extrabuf存储的另一部分数据追加到buffer_
    }
    
    return n;  // 返回读取的总字节数（含 extrabuf 中数据）
}


// inputBuffer_.readFd表示将对端数据读到inputBuffer_中，移动writerIndex_指针
// outputBuffer_.writeFd标示将数据写入到outputBuffer_中，从readerIndex_开始，可以写readableBytes()个字节

// 向fd写入数据   buffer-->fd
ssize_t Buffer::writeFd(int fd, int* saveErrno)
{
    ssize_t n = ::write(fd, peek(), readableBytes()); // 从 peek() 开始写 readableBytes() 字节到 fd
    if (n < 0)
    {
        *saveErrno = errno;
    }
    return n; // 返回写入的字节数
}