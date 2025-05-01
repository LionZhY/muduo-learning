#include <unistd.h>         // close()
#include <sys/types.h>      // 基本系统数据类型
#include <sys/socket.h>
#include <string.h>
#include <netinet/tcp.h>    // TCP_NODELAY选项

#include "Socket.h"
#include "Logger.h"
#include "InetAddress.h"


// 析构
Socket::~Socket()
{
    ::close(sockfd_); // 关闭 socket fd
}


// 将socket fd绑定到指定的本地地址（IP+端口）
void Socket::bindAddress(const InetAddress &localaddr)
{
    // 将 socket fd 绑定到一个本地地址localaddr，如果绑定失败，打印日志并终止程序
    if (0 != ::bind(sockfd_, (sockaddr*)localaddr.getSockAddr(), sizeof(sockaddr_in)))
    {
        LOG_FATAL("bind sockfd:%d fail\n", sockfd_);
    }
}

// 开启监听，准备接收客户端连接
void Socket::listen()
{
    if (0 != ::listen(sockfd_, 1024)) // sockfd_必须是已绑定过的，1024是监听队列的最大长度
    {
        LOG_FATAL("listen sockfd:%d fail\n", sockfd_);
    }
}


// 接受连接
int Socket::accept(InetAddress* peeraddr) // 参数peeraddr 用于存储客户端的地址信息
{
    /**
     * 1. accept函数的参数不合法
     * 2. 对返回的connfd没有设置非阻塞
     * Reactor模型 one loop per thread
     * poller + non-blocking IO
     */
    sockaddr_in addr;                   // 临时变量 addr，用于存储客户端地址
    socklen_t len = sizeof(addr);       // 地址长度，传入 accept4() 时需要指针
    ::memset(&addr, 0, sizeof(addr));   // 清空，避免未初始化字段导致意外

    // fixed : int connfd = ::accept(sockfd_, (sockaddr*)&addr, &len);
    int connfd = ::accept4(sockfd_, (sockaddr*)&addr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC);

    // 如果 connfd 有效（连接成功）
    if (connfd >= 0)
    {
        peeraddr->setSockAddr(addr); // 保存客户端地址信息到调用者传入的 peeraddr 
    }
    
    return connfd;
}


// 关闭写端
void Socket::shutdownWrite()
{
    if (::shutdown(sockfd_, SHUT_WR) < 0) // 调用 Linux 的 shutdown() 系统调用
    {
        LOG_ERROR("shutdownWrite error");
    }
}


// Socket 选项设置

// 启用/禁用Nagle算法
void Socket::setTcpNoDelay(bool on) 
{
    // TCP_NODELAY 用于禁用Nagle算法
    // Nagle算法用于减少网络上传输的小数据报数量
    // 将 TCP_NODELAY 设置为1，可以禁用该算法，允许小数据包立即发送
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
}

// 设置地址重用，允许快速重启服务绑定相同端口
void Socket::setReuseAddr (bool on) 
{
    // SO_REUSEADDR 允许一个套接字强制绑定到一个已被其他套接字使用的端口
    // 这对于需要重启并绑定到相同端口的服务器应用程序非常有用
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
}

// 设置端口复用，允许多个 socket 实例监听同一端口，支持多线程
void Socket::setReusePort (bool on) 
{
    // SO_REUSEPORT 允许同一主机上的多个套接字绑定到相同的端口号
    // 用于在多个线程或进程之间负载均衡传入连接
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
}

// 启用 TCP keepalive 检测对端连接状态
void Socket::setKeepAlive (bool on) 
{
    // SO_KEEPALIVE 启用在已连接的socket上定期传输消息
    // 如果另一端没有响应，则认为连接已断开并关闭
    // 这对于检测网络中失效的对等方非常有用
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
}