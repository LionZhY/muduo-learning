#pragma once 

#include "noncopyable.h"

class InetAddress;

// 封装Socket fd
class Socket : noncopyable
{
public:
    // 构造
    explicit Socket(int sockfd) // 接收一个已创建的socket fd
        : sockfd_(sockfd)
    {
    }

    // 析构
    ~Socket();

    // 获取底层fd
    int  fd() const { return sockfd_; }

    // 绑定本地地址，将socket绑定到localaddr指定的ip和端口
    void bindAddress(const InetAddress &localaddr);

    // 开启监听
    void listen();

    // 接受连接
    int accept(InetAddress* peeraddr); // 参数peeraddr 用于存储客户端的地址信息

    // 关闭写端
    void shutdownWrite();

    // Socket 选项设置
    void setTcpNoDelay(bool on); // 启用/禁用Nagle算法
    void setReuseAddr (bool on); // 设置地址重用，允许快速重启服务绑定相同端口
    void setReusePort (bool on); // 设置端口复用，允许多个 socket 实例监听同一端口，支持多线程
    void setKeepAlive (bool on); // 启用 TCP keepalive 检测对端连接状态

private:
    const int sockfd_;

};