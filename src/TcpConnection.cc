#include <functional>       // std::bind绑定回调函数
#include <string>           
#include <errno.h>
#include <sys/types.h>      // 系统类型 如ssize_t
#include <sys/socket.h>
#include <string.h>
#include <netinet/tcp.h>    // Tcp相关选项
#include <sys/sendfile.h>   // 零拷贝sendfile接口
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

    connectionCallback_(shared_from_this()); // 通知用户连接建立
}


// 连接销毁前调用 由 TcpServer 调用
void TcpConnection::connectDestroyed()
{
    if (state_ == kConnected)
    {
        setState(kDisconnected); // 连接状态设置为已断开
        channel_->disableAll();  // 清除 Poller 中该 channel 对象注册的所有感兴趣事件
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
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &savedErrno); // outbuffer --> fd 
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
    setState(kDisconnected);// 设置状态为已关闭，不再收发数据
    channel_->disableAll(); // 关闭所有感兴趣的事件

    TcpConnectionPtr connPtr(shared_from_this()); // 当前连接对象的 shared_ptr
    connectionCallback_(connPtr); // 执行连接销毁的回调（用户注册）
    
    // must be the last line 必须放在最后，因为closeCallback_内部有可能直接删除TcpConnection
    closeCallback_(connPtr); // 绑定的是TcpServer::removeConnection回调方法 移除连接 删除Channel 
}


// 处理错误事件
void TcpConnection::handleError() 
{
    int optval;
    socklen_t optlen = sizeof optval;
    int err = 0;
    if (::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0 )
    {
        err = errno;
    }
    else 
    {
        err = optval;
    }
    LOG_ERROR("TcpConnection::handleError name:%s - SO_ERROR:%d\n", name_.c_str(), err);
}


/**
 * 发送数据  应用写的快  而内核发送数据慢 需要把待发送数据写入缓冲区 而且设置了水位回调
 */

// 在EventLoop中发送数据
void TcpConnection::sendInLoop(const void* data, size_t len)
{
    ssize_t nwrote = 0;
    ssize_t remaining = len;
    bool faultError = false;

    if (state_ == kDisconnected) // 之前调用过该connection的shutdown 不能再进行发送了
    {
        LOG_ERROR("disconnected, give up writing");
    }

    // 表示channel_第一次开始写数据或者缓冲区没有待发送数据
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        nwrote = ::write(channel_->fd(), data, len);
        if (nwrote >= 0)
        {
            remaining = len - nwrote;
            if (remaining == 0 && writeCompleteCallback_)
            {
                // 既然在这里数据全部发送完成，就不用再给channel设置epollout事件了
                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
        }
        else // nwrote < 0
        {
            nwrote = 0;
            if (errno != EWOULDBLOCK) // EWOULDBLOCK表示非阻塞情况下没有数据后的正常返回 等同于EAGAIN
            {
                LOG_ERROR("TcpConnection::sendInLoop");
                if (errno == EPIPE || errno == ECONNRESET) // SIGPIPE RESET
                {
                    faultError = true;
                }
            }
        }
    }

    /**
     * 说明当前这一次write并没有把数据全部发送出去 剩余的数据需要保存到缓冲区当中
     * 然后给channel注册EPOLLOUT事件，Poller发现tcp的发送缓冲区有空间后会通知
     * 相应的sock->channel，调用channel对应注册的writeCallback_回调方法，
     * channel的writeCallback_实际上就是TcpConnection设置的handleWrite回调，
     * 把发送缓冲区outputBuffer_的内容全部发送完成
     **/

     if (!faultError && remaining > 0)
     {
        // 目前发送缓冲区剩余的待发送的数据的长度
        size_t oldLen = outputBuffer_.readableBytes();
        if (oldLen + remaining >= highWaterMark_ && oldLen < highWaterMark_ && highWaterMarkCallback_)
        {
            loop_->queueInLoop(
                std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));
        }
        outputBuffer_.append((char*)data + nwrote, remaining);
        if (!channel_->isWriting())
        {
            channel_->enableWriting(); // 这里一定要注册channel的写事件 否则poller不会给channel通知epollout
        }
     }

}


// 在EventLoop中执行sendFile
void TcpConnection::sendFileInLoop(int fileDescriptor, off_t offset, size_t count)
{
    ssize_t bytesSent = 0;      // 发送了多少字节数
    ssize_t remaining = count;  // 还要发送多少数据
    bool faultError = false; // 错误的标志位

    if (state_ == kDisconnecting) // 表示此时连接已经断开 就不需要发送数据了
    {
        LOG_ERROR("disconnected, give up writing");
        return;
    }

    // 表示channel第一次开始写数据或者outputBuffer缓冲区中没有数据
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        bytesSent = sendfile(socket_->fd(), fileDescriptor, &offset, remaining);
        if (bytesSent >= 0)
        {
            remaining -= bytesSent;
            if (remaining == 0 && writeCompleteCallback_)
            {
                // remaining为0 意味着数据正好全部发送完 就不需要给其设置写事件的监听
                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
        }
        else // bytesSend < 0
        {
            if (errno != EWOULDBLOCK) // 如果是非阻塞没有数据返回错误这个是正常现象 等同于EAGAIN 否则就异常情况
            {
                LOG_ERROR("TcpConneciton::sendFileInLoop");
            }
            if (errno == EPIPE || errno == ECONNRESET)
            {
                faultError = true;
            }
        }
    }

    // 处理剩余数据
    if (!faultError && remaining > 0)
    {
        // 继续发送剩余数据
        loop_->queueInLoop(
            std::bind(&TcpConnection::sendFileInLoop, shared_from_this(), fileDescriptor, offset, remaining));
    }

}


// 在EventLoop中关闭连接
void TcpConnection::shutdownInLoop()
{
    if (!channel_->isWriting()) // 说明当前outputBuffer_的数据全部向外发送完成
    {
        socket_->shutdownWrite();
    }
}