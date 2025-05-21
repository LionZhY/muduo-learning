#pragma once 

/**
 * 用户使用muduo编写服务器程序
 * 给外边提供编写服务器程序的一个入口
 */

#include <functional>
#include <string>
#include <memory>
#include <atomic>
#include <unordered_map>

#include "EventLoop.h"
#include "Acceptor.h"
#include "InetAddress.h"
#include "noncopyable.h"
#include "EventLoopThreadPool.h"
#include "Callbacks.h"
#include "TcpConnection.h"
#include "Buffer.h"

/**
 * 对外的服务器编程使用的类
 */

class TcpServer
{
public:
    using ThreadInitCallback = std::function<void (EventLoop*)>; // 线程初始化回调类型

    // 枚举类型：是否启用SO_REUSEPORT重用端口
    enum Option
    {
        kNoReusePort, // 不允许重用本地端口
        kReusePort,   // 允许重用本地端口
    };

    TcpServer(EventLoop* loop,              // 主EventLoop
              const InetAddress& listenAddr,// 监听地址
              const std::string& nameArg,   // 服务器名称
              Option option = kNoReusePort);// 端口复用选项
    
    ~TcpServer();

    // 设置用户层定义的回调 => 存入成员变量
    void setThreadInitCallback(const ThreadInitCallback& cb) { threadInitCallback_ = cb; } // subloop初始化时的用户回调
    void setConnectionCallback(const ConnectionCallback& cb) { connectionCallback_ = cb; } // 连接回调
    void setMessageCallback   (const MessageCallback& cb)    { messageCallback_ = cb; }    // 消息处理回调
    void setWriteCommpleteCallback(const WriteCompleteCallback& cb) { writeCompleteCallback_ = cb; } // 写完成回调

    // 设置线程池中线程数量(底层subloop个数)   
    void setThreadNum (int numThreads); // 默认为0 即所有事件都在主线程处理
    /**
     * 如果没有监听，就启动服务器（监听）
     * 多次调用没有副作用
     * 线程安全
     */

    // 启动 TcpServer（只可调用一次），开启 Acceptor 监听，启动线程池
    void start();

private:
    // 新连接建立后的回调
    void newConnection(int sockfd, const InetAddress& peerAddr);
    // Tcp连接关闭的回调
    void removeConnection(const TcpConnectionPtr& conn);
    // 在subloop中执行连接销毁
    void removeConnectionInLoop(const TcpConnectionPtr& conn);

    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>; // <连接名，TcpConnectionPtr>
    ConnectionMap connections_; // 当前所有的Tcp连接

    EventLoop* loop_; // baseloop(mianloop) 用户自定义的loop

    const std::string ipPort_; // 监听地址
    const std::string name_;   // 服务器名称，用户自定义

    std::unique_ptr<Acceptor> acceptor_; // 智能指针管理Acceptor  运行在mainLoop负责监听新连接事件
    std::shared_ptr<EventLoopThreadPool> threadPool_; // 管理一个线程池 one loop per thread

    ConnectionCallback connectionCallback_;       // 连接建立or断开的回调
    MessageCallback messageCallback_;             // 有读写事件发生时的回调
    WriteCompleteCallback writeCompleteCallback_; // 消息发送完成后的回调

    ThreadInitCallback threadInitCallback_; // 用户传入的subloop线程初始化时的回调

    int numThreads_;          // 线程池中线程的数量
    std::atomic_int started_; // 服务器是否已启动
    int nextConnId_;          // 为每个连接生成唯一表示（拼接成连接名）
     
};