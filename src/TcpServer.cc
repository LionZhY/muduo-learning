#include <functional>
#include <string.h>

#include "TcpServer.h"
#include "Logger.h"
#include "TcpConnection.h"

// 工具函数：检查Loop不为空
static EventLoop* CheckLoopNotNull(EventLoop* loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d mainLoop is null!\n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}


TcpServer::TcpServer(EventLoop* loop,              // mainLoop指针
                     const InetAddress& listenAddr,// 监听地址
                     const std::string& nameArg,   // 服务器名称
                     Option option) // 端口复用选项(默认= kNoReusePort)
    : loop_(CheckLoopNotNull(loop))  
    , ipPort_(listenAddr.toIpPort()) // 监听地址 IP:Port
    , name_(nameArg) 
    , acceptor_(new Acceptor(loop, listenAddr, option == kNoReusePort)) // 创建Acceptor对象
    , threadPool_(new EventLoopThreadPool(loop, name_)) // 初始化线程池对象
    , connectionCallback_() 
    , messageCallback_()
    , nextConnId_(1) // 用于生成连接的唯一ID
    , started_(0)
{
    // 设置新用户连接时的回调：
    // Acceptor监听到有新连接到来，在handleRead()执行回调，这里将回调设置为TcpServer::newConnection
    acceptor_->setNewConnectionCallback(
        std::bind(&TcpServer::newConnection, this, std::placeholders::_1, std::placeholders::_2) );
}

TcpServer::~TcpServer()
{
    // 清理所有Tcp连接
    for (auto &item : connections_) // 每个 item 是一个 <连接名, TcpConnectionPtr> 键值对
    {
        TcpConnectionPtr conn(item.second); // 取出原有的 TcpConnectionPtr，赋值给局部变量 conn
        item.second.reset();                // 原来的 TcpConnectionPtr 清空（释放引用）
        
        // 分发到对应 subLoop 中执行连接销毁操作 TcpConnection::getLoop()
        conn->getLoop()->runInLoop(
            std::bind(&TcpConnection::connectDestroyed, conn) );; // 绑定了conn的销毁函数 connectDestroyed()
    }
}


// 设置线程池中线程数量 (底层subloop个数)   
void TcpServer::setThreadNum (int numThreads) // 默认为0 即所有事件都在主线程处理
{
    numThreads_ = numThreads;
    threadPool_->setThreadNum(numThreads_);
}


// 启动 TcpServer（只可调用一次），开启 Acceptor 监听，启动线程池
void TcpServer::start()
{
    if (started_.fetch_add(1) == 0) // 只有第一次调用会执行以下代码 防止一个TcpServer对象被start多次
    {
        threadPool_->start(threadInitCallback_); // 启动底层的loop线程池
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get())); // 启动监听
    }
}


// 新连接建立后的处理
void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr)
{
    // 传入accept后得到的客户端Socketfd  客户端的地址peerAddr

    /**
     * 在有新连接建立时由 Acceptor 触发的回调函数
     * 负责将mainloop接收到的请求连接(acceptChannel_会有读事件发生) 通过回调轮询分发给subloop去处理
     */
    
    // 从线程池中选择一个subloop
    EventLoop* ioLoop = threadPool_->getNextLoop(); 

    // 构造唯一的连接名称connName 如 "MyServer-127.0.0.1:8080#1"
    char buf[64] = {0}; 
    snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_); // 监听地址#连接计数 如：-127.0.0.1:8000#1
    ++nextConnId_; // 连接ID自增 
    std::string connName = name_ + buf; // 拼接 服务器名称-127.0.0.1:8080#1

    // 打印连接建立日志  服务器名 连接名 客户端IP:Port
    LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from %s\n", 
              name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str());
    
    // 获取sockfd绑定的本地地址localAddr（服务端）
    sockaddr_in local; // 保存本地地址信息（服务端）
    ::memset(&local, 0, sizeof(local));
    socklen_t addrlen = sizeof(local);
    if (::getsockname(sockfd, (sockaddr*)&local, &addrlen) < 0) // 调用 getsockname() 获取sockfd对应的本地地址
    {
        LOG_ERROR("sockets::getLocalAddr"); // 调用失败 打印日志
    }
    InetAddress localAddr(local); // 将获取的 sockaddr_in 包装成 InetAddress 类对象
    
    // 创建TcpConnectionPtr智能指针 conn
    TcpConnectionPtr conn( new TcpConnection(ioLoop,     
                                             connName,   
                                             sockfd,
                                             localAddr,  // 服务端地址（本地地址）
                                             peerAddr) );// 客户端地址           

    // 将连接加入Tcp连接的map中
    connections_[connName] = conn; // <连接名，TcpConnectionPtr>

    // 用户设置给具体连接TcpConnection的回调
    conn->setConnectionCallback(connectionCallback_);       // 连接建立/关闭时的回调
    conn->setMessageCallback(messageCallback_);             // 消息到达时的回调
    conn->setWriteCompleteCallback(writeCompleteCallback_); // 数据写入完成时的回调
    conn->setCloseCallback(
        std::bind(&TcpServer::removeConnection, this, std::placeholders::_1) ); // 关闭连接时的回调
    
    // 将连接建立的后续工作TcpConnection::connectEstablished封装成一个任务，扔进 subLoop 的事件循环中去执行
    ioLoop->runInLoop(
        std::bind(&TcpConnection::connectEstablished, conn) );
}


// Tcp连接关闭的回调
void TcpServer::removeConnection(const TcpConnectionPtr& conn)
{
    loop_->runInLoop( // removeConnectionInLoop()绑定conn 作为任务传给mainLoop
        std::bind(&TcpServer::removeConnectionInLoop, this, conn) ); 
}


// 在subloop中执行连接销毁 由mainLoop调用
void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn)
{
    // 打印日志：服务器名称 连接名称
    LOG_INFO("TcpServer::removeConnectionInLoop [%s] - connection %s\n",
             name_.c_str(), conn->name().c_str());
    
    // 从Connectionmap中删除当前连接conn
    connections_.erase(conn->name()); 

    // 向subLoop提交connectDestroyed()任务 subLoop执行请清理资源
    EventLoop* ioLoop = conn->getLoop(); 
    ioLoop->queueInLoop( 
        std::bind(&TcpConnection::connectDestroyed, conn) ); 
}