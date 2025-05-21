#include <functional>       // std::bind绑定回调函数
#include <string>           
#include <errno.h>
#include <sys/types.h>      // 系统类型 如ssize_t
#include <sys/socket.h>
#include <string.h>
#include <netinet/tcp.h>    // Tcp相关选项
#include <sys/sendfile.h>   // 提供sendfile()
#include <fcntl.h>          // 提供open函数以及文件打开的flag
#include <unistd.h>         // close read write函数

#include "TcpConnection.h"
#include "Logger.h"
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"

// 辅助函数 检查EventLoop是否为null
static EventLoop* CheckLoopNotNull(EventLoop* loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d mainLoop is null!\n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}


TcpConnection::TcpConnection(EventLoop* loop,               // 连接所属的EventLoop
                             const std::string& nameArg,    // 连接名
                             int sockfd,                    // 连接对应的sockfd
                             const InetAddress& localAddr,  // 本地地址（服务器端）
                             const InetAddress& peerAddr)   // 对端地址（客户端）
    : loop_(CheckLoopNotNull(loop)) 
    , name_(nameArg)                
    , state_(kConnecting)           // 初始状态为正在建立连接
    , reading_(true)                // 默认监听可读事件
    , socket_(new Socket(sockfd))   // sockfd封装成socket对象
    , channel_(new Channel(loop, sockfd)) // 为该连接创建channel fd与loop绑定
    , localAddr_(localAddr) 
    , peerAddr_(peerAddr)   
    , highWaterMark_(64 * 1024 * 1024) // 写缓冲区高水位标记 64M

{
    // 设置channel相应事件的回调函数  当sockfd上事件发生 -> 回调TcpConnection::handlexxx
    channel_->setReadCallback ( std::bind(&TcpConnection::handleRead, this, std::placeholders::_1) ); // 读事件回调 有数据可读时触发
    channel_->setWriteCallback( std::bind(&TcpConnection::handleWrite, this) ); // 写事件回调 发送缓冲区可写
    channel_->setcloseCallback( std::bind(&TcpConnection::handleClose, this) ); // 关闭事件回调 对端关闭连接
    channel_->setErrorCallback( std::bind(&TcpConnection::handleError, this) ); // 错误事件回调 

    // 记录连接名，fd
    LOG_INFO("TcpConnection::ctor[%s] at fd=%d\n", name_.c_str(), sockfd);
    
    // 启用TCP保活机制 Socket::setKeepAlive()
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
    // 只记录日志 不做额外资源释放，具体资源释放由shared_ptr和Channel::remove()管理
    LOG_INFO("TcpConnection::dtor[%s] at fd=%d state=%d\n", 
              name_.c_str(), channel_->fd(), (int)state_); // 连接名 fd 连接状态
}


// 发送数据  选择是在当前线程直接发送，还是将发送任务转移到所属的 I/O 线程执行
void TcpConnection::send(const std::string& buf)
{
    // 只有在连接建立完成，双方互通，才能发送数据
    if (state_ == kConnected) 
    {  
        // 检查当前线程是否就是这个连接所属的 EventLoop 所在线程
        if (loop_->isInLoopThread()) 
        {
            sendInLoop(buf.c_str(), buf.size());
            // 这种是对于单个reactor的情况，用户调用conn->send时，loop_即为当前线程
        }
        else // 否则，通过 runInLoop() 将任务加入事件循环队列，由 IO 线程执行
        {
            loop_->runInLoop(
                std::bind(&TcpConnection::sendInLoop, this, buf.c_str(), buf.size()));
        }
    }

    // state_ != kConnected 连接未建立或已关闭，不做任何处理
}


// 发送文件内容  线程选择
void TcpConnection::sendFile(int fileDescriptor, // 文件描述符，必须是一个已打开的普通文件
                             off_t offset, // 文件起始偏移
                             size_t count) // 要发送的字节数
{
    // 只有在连接已建立（state_ == kConnected）的状态下，才能发送数据
    if (connected())
    {
        if (loop_->isInLoopThread()) // 判断当前线程是否是loop循环的线程
        {
            sendFileInLoop(fileDescriptor, offset, count);
        }
        else // 如果不是 则唤醒运行这个TcpConnection的线程执行loop循环
        {
            loop_->runInLoop(
                std::bind(&TcpConnection::sendFileInLoop, shared_from_this(), // shared_from_this() 保证当前对象在任务执行前不会被销毁
                          fileDescriptor, offset, count));
        }
    }
    else // 连接未建立 记录错误日志
    {
        LOG_ERROR("TcpConnection::sendFile - not connected");
    }
}

// 半关闭连接
void TcpConnection::shutdown() // 关闭写端，不再发送数据，但仍可接收数据
{
    // 确保连接状态为 kConnected（已建立连接）
    if (state_ == kConnected)
    {
        setState(kDisconnecting); // 修改连接状态为正在关闭
        loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this)); // 将shutdownInLoop()提交到所属线程的事件循环
    }
}


// 连接建立后 由 TcpServer 调用
void TcpConnection::connectEstablished()
{
    setState(kConnected);              // 修改连接状态为 已连接
    channel_->tie(shared_from_this()); // 将当前连接的shared_ptr绑定给该连接的Channel
    channel_->enableReading();         // 向poller注册该连接的fd的读事件 EPOLLIN

    // 执行用户注册的“连接建立”回调
    connectionCallback_(shared_from_this()); // 通知用户（业务代码层）连接建立 
}


// 连接销毁前调用 由 TcpServer 调用
void TcpConnection::connectDestroyed()
{
    if (state_ == kConnected)
    {
        setState(kDisconnected); // 连接状态设置为已断开
        channel_->disableAll();  // 清除channel所有感兴趣事件
        connectionCallback_(shared_from_this()); // 通知用户连接销毁
    }

    channel_->remove(); // 从 Poller 中移除 Channel
}


/* handlexxx() => Channel::setReadCallback()作为Channel检测到事件后的回调 => Channel::handleEventWithGuard()中被调用 */

// 处理读事件 fd-->inputbuffer  就是Channel 检测到 EPOLLIN（可读事件） 后的回调
// 读是相对服务器而言的，当对端客户有数据到达，服务器端检测EPOLLIN，就会触发该fd上的回调，handleRead读走对端发来的数据
void TcpConnection::handleRead(Timestamp receiveTime) 
{
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno); // fd--> inputbuffer
    if (n > 0) // 有数据被读取成功
    {
        // 已建立连接的用户有可读事件发生了 调用用户传入的消息处理回调
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }
    else if (n == 0) // 对端关闭连接 处理连接关闭
    {
        handleClose();
    }
    else // n < 0 出错了 处理错误流程
    {
        errno = savedErrno;
        LOG_ERROR("TcpConnection::handleRead");
        handleError();
    }
}

// 处理写事件  outbuffer --> fd 
void TcpConnection::handleWrite() 
{
    if (channel_->isWriting()) // 判断当前Channel是否监听写事件 EPOLLOUT
    {
        int savedErrno = 0;

        // outbuffer --> fd 
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &savedErrno); 
        if (n > 0) // 写入成功
        {
            outputBuffer_.retrieve(n); // 从缓冲区中读取readable区域的数据 移动readerIndex_
            // 如果待发送的数据已经全部写完
            if (outputBuffer_.readableBytes() == 0) 
            {
                channel_->disableWriting();  // 关闭写事件监听 写缓冲区已空，不再需要监听 EPOLLOUT
                if (writeCompleteCallback_)  // 若设置了写完成回调 投递到所属 EventLoop 中延迟执行
                {
                    loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
                }
                if (state_ == kDisconnecting) // 若处于“半关闭”状态，此时满足!channel_->isWriting()，真正关闭连接
                {
                    shutdownInLoop();
                }
            }
        }
        else // n <= 0 写入失败，记录错误日志
        {
            LOG_ERROR("TcpConnection::handleWrite");
        }
    }
    else // 当前Channel没有监听写事件却调用了 handleWrite()，是异常情况
    {
        LOG_ERROR("TcpConnection fd=%d is down, no more writing", channel_->fd());
    }
}


// 处理连接关闭事件
void TcpConnection::handleClose() 
{
    LOG_INFO("TcpConneciton::handleClose fd=%d state=%d\n", channel_->fd(), (int)state_);

    setState(kDisconnected);        // 设置状态为已关闭，不再收发数据
    channel_->disableAll();         // 关闭所有感兴趣的事件

    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr);   // 用户设置的连接状态回调（通知用户连接状态发生变化）
    
    // must be the last line 必须放在最后，因为closeCallback_内部有可能直接删除TcpConnection
    closeCallback_(connPtr);        // 服务端设置的关闭连接回调 绑定TcpServer::removeConnection回调方法
}


// 处理错误事件
void TcpConnection::handleError() 
{
    int optval; // 用来存储getsockopt()返回的错误码
    socklen_t optlen = sizeof optval; 
    int err = 0;// 存储getsockopt()失败的错误码

    // getsocket() 通过SO_ERROR选项读取Socket错误状态
    if (::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0 )
    {
        err = errno; // 如果调用失败（极少），内核设置失败原因errno，使用 errno 作为错误码
    }
    else 
    {
        err = optval;// 如果成功，说明 optval 中就是 socket 的错误码
    }

    // 打印连接名 和对应的错误码
    LOG_ERROR("TcpConnection::handleError name:%s - SO_ERROR:%d\n", name_.c_str(), err);
}


/**
 * 发送数据  应用写的快  而内核发送数据慢 需要把待发送数据写入缓冲区 而且设置了水位回调
 */

// 在EventLoop中发送数据
void TcpConnection::sendInLoop(const void* data, size_t len)
{
    ssize_t nwrote = 0;      // 实际写到Socket的字节数
    ssize_t remaining = len; // 剩余还未写的数据长度
    bool faultError = false; // 标记是否出现致命错误

    // 若连接断开，不再发送，记录日志
    if (state_ == kDisconnected) 
    {
        LOG_ERROR("disconnected, give up writing");
    }

    /**
     * sendInLoop 先尝试直接写 socket，
     * 若未写完则将剩余数据存入 outputBuffer，并注册写事件
     * epoll 通知 socket 可写时调用handleWrite()，继续把 outputBuffer_ 中的数据写入 socket，直到写完
     */


    // 如果当前 channel 没有注册写事件，且 outputBuffer_ 为空，表示可以尝试直接写 socket 
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0) 
    {
        // 尝试直接将 data 写入 socket （不用缓冲区）
        nwrote = ::write(channel_->fd(), data, len);

        if (nwrote >= 0) // 写入成功
        {
            remaining = len - nwrote; // 更新剩余未写长度
            if (remaining == 0 && writeCompleteCallback_) // 如果全部写完，且用户设置了写完成回调
            {
                // 写完成回调需要在所属 EventLoop 中执行
                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
        }
        else // nwrote < 0 写入失败
        {
            nwrote = 0; // 写入字节数置0
            if (errno != EWOULDBLOCK) // 若失败原因不是 “资源暂时不可用”，即真正的写错误
            {
                LOG_ERROR("TcpConnection::sendInLoop");
                // 如果是 EPIPE（对端已关闭写）或 ECONNRESET（对端复位连接），标记出现错误
                if (errno == EPIPE || errno == ECONNRESET) // SIGPIPE RESET
                {
                    faultError = true;
                }
            }
        }
    }

    /**
     * 若还有未发送的数据（remaining > 0）且没有发生严重错误：
     * - 说明当前这一次write并没有把数据全部发送出去，剩余的数据需要保存到outputbuffer中
     * - 然后给channel注册EPOLLOUT写事件，以便后续在 handleWrite 中继续发送
     * 
     * Poller发现tcp的发送缓冲区有可读空间后，通知相应的sock->channel => 调用channel对应注册的writeCallback_
     * channel的writeCallback_就是TcpConnection设置的handleWrite，把outputBuffer_的内容全部发送
     **/

    if (!faultError && remaining > 0)
    {
       size_t oldLen = outputBuffer_.readableBytes(); // 原先outputbuffer剩余待发送的数据的长度

       // 如果写入后超过了高水位线，且原本没超过，则触发高水位回调
       if (oldLen + remaining >= highWaterMark_ && oldLen < highWaterMark_ && highWaterMarkCallback_)
       {
           loop_->queueInLoop(
               std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));
       }

       // 将未写完的数据(data + nwrote之后长remianing的数据) 追加到 outputbuffer
       outputBuffer_.append((char*)data + nwrote, remaining);

       // 如果之前没有注册写事件，现在需要注册
       if (!channel_->isWriting())
       {
           channel_->enableWriting(); 
           // 注册写事件，等待 poller 通知 socket 可写时执行 handleWrite
           // 这里一定要注册channel的写事件 否则poller不会给channel通知epollout
       }
    }
    
}


// 在EventLoop中执行sendFile
void TcpConnection::sendFileInLoop( int fileDescriptor, // 要发送的源文件的fd
                                    off_t offset,       // 源文件中的偏移量，从offset开始发送
                                    size_t count)       // 最多发送多少字节
{
    ssize_t bytesSent = 0;      // 实际发送的字节数
    ssize_t remaining = count;  // 还有多少字节要发送（初始为全部）
    bool faultError = false;    // 是否发生了致命错误

    // 表示此时连接已经断开 就不需要发送数据了
    if (state_ == kDisconnecting) 
    {
        LOG_ERROR("disconnected, give up writing");
        return;
    }

    // 当前没有监听写事件，且 outputBuffer_ 中没有待发送的数据
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        // 调用sendfile 系统调用  fileDescriptor指向的文件内容 --> socket fd
        bytesSent = sendfile(socket_->fd(), fileDescriptor, &offset, remaining); 

        if (bytesSent >= 0) // 成功发送 bytesSent字节
        {
            remaining -= bytesSent; 
            // 如果全部写完，并设置了写完成回调，投递到所属EventLoop中执行
            if (remaining == 0 && writeCompleteCallback_)
            {
                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
        }
        else // bytesSend < 0 发送失败
        {
            if (errno != EWOULDBLOCK) // 如果是非阻塞 socket 的正常现象（写缓冲区满）否则就异常情况
            {
                LOG_ERROR("TcpConneciton::sendFileInLoop");
            }
            if (errno == EPIPE || errno == ECONNRESET) // EPIPE（写端对方已关闭）或 ECONNRESET（连接被重置）
            {
                faultError = true;
            }
        }
    }

    // 如果还有剩余数据，且没有致命错误
    if (!faultError && remaining > 0)
    {
        // 递归地将 sendFileInLoop 继续投递进 EventLoop 中处理
        loop_->queueInLoop(
            std::bind(&TcpConnection::sendFileInLoop, shared_from_this(), fileDescriptor, offset, remaining));
    }

}


// 在EventLoop中关闭连接写端
void TcpConnection::shutdownInLoop()
{
    // Channel没有在监听写事件 说明当前outputBuffer_的数据全部向外发送完成，可以关闭写端
    if (!channel_->isWriting()) 
    {
        socket_->shutdownWrite(); // 调用Socket封装的 shutdown(SHUT_WR)，关闭写端，触发半关闭
    }

    // 如果channel还在监听 EPOLLOUT 写事件，表示还有数据未写入 socket，不能立即关闭写端。
}