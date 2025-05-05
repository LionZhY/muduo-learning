#include <functional>
#include <string.h>

#include "TcpServer.h"
#include "Logger.h"
#include "TcpConnection.h"


static EventLoop* CheckLoopNotNull(EventLoop* loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d mainLoop is null!\n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}


TcpServer::TcpServer(EventLoop* loop,              // 主EventLoop
                     const InetAddress& listenAddr,// 监听地址
                     const std::string& nameArg,   // 服务器名称
                     Option option = kNoReusePort) // 端口复用选项
    : loop_(CheckLoopNotNull(loop))
    , ipPort_(listenAddr.toIpPort())
    , name_(nameArg)
    , acceptor_(new Acceptor(loop, listenAddr, option == kNoReusePort))
    , threadPool_(new EventLoopThreadPool(loop, name_))
    , connectionCallback_()
    , messageCallback_()
    , nextConnId_(1)
    , started_(0)
{
    // 当有新用户连接时，Acceptor类中绑定的acceptChannel_会有读事件发生，执行handleRead()，调用TcpServer::newConnection回调
    acceptor_->setNewConnectionCallback(
        std::bind(&TcpServer::newConnection, this, std::placeholders::_1, std::placeholders::_2) );
}

TcpServer::~TcpServer()
{
    for (auto &item : connections_)
    {
        TcpConnectionPtr conn(item.second);
        item.second.reset(); // 把原始的智能指针复位，让栈空间的TcpConnectionPtr conn指向该对象  当conn出了其作用域 即可释放智能指针指向的对象
        // 销毁连接
        conn->getLoop()->runInLoop(
            std::bind(&TcpConnection::connectDestroyed, conn) );;
    }

}


// 设置线程池中线程数量 (底层subloop个数)   
void TcpServer::setThreadNum (int numThreads) // 默认为0 即所有事件都在主线程处理
{
    int numThreads_ = numThreads;
    threadPool_->setThreadNum(numThreads_);
}


// 启动 TcpServer（只可调用一次），开启 Acceptor 监听，启动线程池
void TcpServer::start()
{
    if (started_.fetch_add(1) == 0) // 防止一个TcpServer对象被start多次
    {
        threadPool_->start(threadInitCallback_); // 启动底层的loop线程池
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
    }
}


// 新连接建立后的处理
void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr)
{
    /**
     * 有一个新用户连接，acceptor会执行这个回调
     * 负责将mainloop接收到的请求连接(acceptChannel_会有读事件发生) 通过回调轮询分发给subloop去处理
     */
    
    // 轮询算法 选择一个subloop来管理connfd对应的channel
    EventLoop* ioLoop = threadPool_->getNextLoop();
    char buf[64] = {0};
    snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_; // 这里没有设置原子类 是因为其只在mainloop中执行 不涉及线程安全问题
    std::string connName = name_ + buf;

    LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from %s\n",
              name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str());
    
    // 通过sockfd获取其绑定的本机的ip地址和端口信息
    sockaddr_in local;
    ::memset(&local, 0, sizeof(local));
    socklen_t addrlen = sizeof(local);
    if (::getsockname(sockfd, (sockaddr*)&local, &addrlen) < 0)
    {
        LOG_ERROR("sockets::getLocalAddr");
    }

    InetAddress localAddr(local);
    TcpConnectionPtr conn(new TcpConnection(ioLoop,
                                            connName,
                                            sockfd,
                                            localAddr,
                                            peerAddr));

    connections_[connName] = conn;

    // 下面的回调是用户设置给TcpServer => TcpConnection的，
    // 至于Channel绑定的则是TcpConnection设置的四个，handleRead,handleWrite... 这下面的回调用于handlexxx函数中
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);

    // 设置了如何关闭连接的回调
    conn->setCloseCallback(
        std::bind(&TcpServer::removeConnection, this, std::placeholders::_1) );
    
    ioLoop->runInLoop(
        std::bind(&TcpConnection::connectEstablished, conn) );
}


// 连接关闭请求
void TcpServer::removeConnection(const TcpConnectionPtr& conn)
{
    loop_->runInLoop(
        std::bind(&TcpServer::removeConnectionInLoop, this, conn) );
}


// 在subloop中执行连接销毁
void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn)
{
    LOG_INFO("TcpServer::removeConnectionInLoop [%s] - connection %s\n",
             name_.c_str(), conn->name().c_str());
    
    connections_.erase(conn->name());
    EventLoop* ioLoop = conn->getLoop();
    ioLoop->queueInLoop(
        std::bind(&TcpConnection::connectDestroyed, conn) );
    
}