#include <string>

#include "TcpServer.h"
#include "Logger.h"

/**
 * 实现一个典型的回显服务器 EchoServer ：
 * 客户端发送消息 --> onMessage触发 服务器收到消息 --> 服务器把消息原样再发送回客户端Socket --> 客户端从Socket读取
 * 
 * 回显服务器不用于实际业务，主要用于验证测试：网络通信功能，检查TCP连接
 */


// 自定义的服务器类 封装TcpServer
class EchoServer
{
public:
    EchoServer( EventLoop* loop,         // mainLoop
                const InetAddress& addr, // 监听地址 ip/端口
                const std::string& name) // 服务器名称
        : server_(loop, addr, name)
        , loop_(loop)
    {
        // 设置连接建立/断开的回调  onConnection -> TcpServer
        server_.setConnectionCallback( 
            std::bind(&EchoServer::onConnection, this, 
                      std::placeholders::_1) ); // 连接指针TcpConnectionPtr
        
        // 设置消息到达的回调  onMessage -> TcpServer
        server_.setMessageCallback(    
            std::bind(&EchoServer::onMessage, this, 
                      std::placeholders::_1,    // 连接指针TcpConnectionPtr
                      std::placeholders::_2,    // 接收buffer
                      std::placeholders::_3) ); // 时间戳Timestamp 表示接收到数据的时间
        
        // 设置subLoop线程数量
        server_.setThreadNum(3); // 设置3 个 subloop线程，即1个mainLoop + 3个subLoop
    }

    // 启动服务器
    void start()
    {
        server_.start(); // TcpServer::start()
    }


private:
    // 连接建立或断开时触发
    void onConnection(const TcpConnectionPtr& conn)
    {
        if (conn->connected()) // 打印连接信息
        {
            // peerAddress 获取对端地址 ip:端口
            LOG_INFO("Conneciton UP : %s", conn->peerAddress().toIpPort().c_str());
        }
        else // 打印断开信息
        {
            LOG_INFO("Connection DOWN : %s", conn->peerAddress().toIpPort().c_str());
        }
    }

    // 客户端发送数据到来时触发
    void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp time)
    {
        /* 回显 客户端发来什么，就立即原样发回*/
        std::string msg = buf->retrieveAllAsString(); // 接收缓冲区内容提取为字符串，同时清空buffer
        conn->send(msg); // 将接收的消息原样发送回客户端
        // conn->shutdown()  // 关闭写端  底层相应EPOLLHUP => 执行closeCallbak_
    }

    TcpServer server_;
    EventLoop* loop_;
};


int main()
{
    EventLoop loop;         // 创建mainLoop
    InetAddress addr(8080); // 创建监听地址 等价于 InetAddress("0.0.0.0", 8080)
                            // 表示监听本机上的所有网络接口（IP地址）的 8080 端口

    EchoServer server(&loop, addr, "EchoServer"); // 构造EchoServer对象

    server.start(); // 启动服务器 开启监听
    loop.loop();    // 开启事件循环 主线程阻塞等待事件

    return 0;
}