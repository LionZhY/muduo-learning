#include <string>

#include "TcpServer.h"
#include "Logger.h"

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
        // 注册回调：有新连接或连接断开
        server_.setConnectionCallback( 
            std::bind(&EchoServer::onConnection, this, std::placeholders::_1) );
        
        // 注册回调：有新数据到达时
        server_.setMessageCallback(    
            std::bind(&EchoServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3) );;
        
        // 设置合适的subLoop线程数量
        server_.setThreadNum(3); // 即会有 1 个主 IO 线程 + 3 个 subloop
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
        std::string msg = buf->retrieveAllAsString(); // 接收缓冲区内容提取未字符串，同时清空buffer
        conn->send(msg); // 将接收的消息原样发送回客户端
        // conn->shutdown()  // 关闭写端  底层相应EPOLLHUP => 执行closeCallbak_
    }

    TcpServer server_;
    EventLoop* loop_;
};


int main()
{
    EventLoop loop;         // 创建mainLoop
    InetAddress addr(8080); // 创建监听地址 监听本地8080端口

    EchoServer server(&loop, addr, "EchoServer"); // 构造EchoServer对象

    server.start(); // 启动服务器 开启监听
    loop.loop();    // 开启事件循环 主线程阻塞等待事件

    return 0;
}