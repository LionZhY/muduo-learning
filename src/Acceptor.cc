#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h> // 提供 POSIX 操作系统 API，如 close()

#include "Acceptor.h"
#include "Logger.h"
#include "InetAddress.h"

// 创建非阻塞监听 socket 的辅助函数
static int createNonblocking()
{
    // 创建一个非阻塞、自动关闭的 TCP socket
    int sockfd = ::socket(AF_INET,      // IPv4地址族
                          SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, // 面向连接TCP | 非阻塞 | exec()系列系统调用时自动关闭fd
                          IPPROTO_TCP); // 明确指定TCP协议
    
    // 创建失败，记录致命错误并中止程序运行
    if (sockfd < 0)
    {
        // 出错的文件:函数:行号 + 错误码errno
        LOG_FATAL("%s:%s:%d listen socket create err:%d\n", __FILE__, __FUNCTION__, __LINE__, errno);
    }
    // 返回新创建的socket fd
    return sockfd;
}

// 构造  传入 mainLoop指针loop  指定监听地址listenAddr  是否设置SO_REUSEPORT标志reuseport
Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport)
    : loop_(loop)                               // 保存传入的mainLoop指针
    , acceptSocket_(createNonblocking())        // 创建非阻塞 TCP socket (listenfd)
    , acceptChannel_(loop, acceptSocket_.fd())  // 将该 acceptSocket_ 封装成 Channel
    , listenning_(false)                        // 初始为 false，尚未监听
{
    // 设置socket选项
    acceptSocket_.setReuseAddr(true); // 允许地址重复使用
    acceptSocket_.setReusePort(true); // 允许多个socket监听同一端口 IP:Port

    // 绑定socket到指定的地址(IP+Port)
    acceptSocket_.bindAddress(listenAddr);

    // 设置读事件的回调  当listenfd可读（新事件到来），由EventLoop调用handleRead()
    acceptChannel_.setReadCallback(
        std::bind(&Acceptor::handleRead, this)); // 将handleRead()绑定为读事件回调函数
}


Acceptor::~Acceptor()
{
    acceptChannel_.disableAll(); // 把Poller中感兴趣的事件都取消监听
    acceptChannel_.remove();     // 删除Channel
}



// 启动监听本地端口
void Acceptor::listen()
{
    listenning_ = true;
    acceptSocket_.listen();         // socket 开始接收连接
    acceptChannel_.enableReading(); // 设置 acceptChannel_ 感兴趣的事件为 EPOLLIN（读事件）
}


// 处理新用户连接事件的回调 (acceptChannel_ 注册的“读事件回调函数”)
void Acceptor::handleRead() // 当 listenfd 可读（有新连接到达）时，EventLoop 会回调本函数
{
    InetAddress peerAddr; // 保存客户端的地址信息 IP+端口

    // 调用 accept() 获取新连接
    int connfd = acceptSocket_.accept(&peerAddr); // connfd 是和客户端通信的 fd
    if (connfd >= 0)
    {
        if (NewConnectionCallback_) // NewConnectionCallback_由TcpServer提供，如果设置了就直接调用
        {
            NewConnectionCallback_(connfd, peerAddr); // 轮询找到subloop，唤醒并分发当前的新客户端的Channel
        }
        else
        {
            ::close(connfd); 
        }
    }
    else 
    {
        // 打印错误日志：文件:函数:行 + 错误码errno
        LOG_ERROR("%s:%s:%d accept err:%d\n", __FILE__, __FUNCTION__, __LINE__, errno);
        // 如果错误是EMFILE：当前进程打开的fd数量达到上线
        if (errno == EMFILE)
        {
            LOG_ERROR("%s:%s:%d sockfd reached limit\n", __FILE__, __FUNCTION__, __LINE__);
        }
    }
}