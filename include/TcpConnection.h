#pragma once

#include <memory>
#include <string>
#include <atomic>

#include "noncopyable.h"
#include "InetAddress.h"
#include "Callbacks.h"
#include "Buffer.h"
#include "Timestamp.h"

class Channel;
class EventLoop;
class Socket;

/**
 * TcpServer => Acceptor => 有一个新用户连接，通过accept函数拿到connfd
 * => TcpConnection设置回调 => 设置到Channel => Poller => Channel回调
 */


class TcpConnection : noncopyable, 
                      public std::enable_shared_from_this<TcpConnection> // 继承enable_shared_from_this 允许对象内部获取shared_ptr<TcpConnection>
{
public:
    TcpConnection(EventLoop* loop,
                  const std::string& nameArg,
                  int sockfd,
                  const InetAddress& localAddr,
                  const InetAddress& peerAddr);

    ~TcpConnection();

    // 获取EventLoop
    EventLoop* getLoop() const { return loop_; }
    // 获取连接名称
    const std::string& name() const { return name_; }
    // 获取本地地址
    const InetAddress& localAddress() const { return localAddr_; }
    // 获取对端地址
    const InetAddress& peerAddress() const { return peerAddr_; }
    // 判断是否已连接
    bool connected() const { return state_ == kConnected; }

    // 发送数据
    void send(const std::string& buf); // 向对端发送字符串数据
    void sendFile(int fileDescriptor, off_t offset, size_t count); // 向对端发送文件中的部分数据

    // 主动关闭连接（半关闭连接）
    void shutdown();

    // 用户设置回调 TcpServer中设置
    void setConnectionCallback(const ConnectionCallback& cb)        // 新连接建立时的回调      
    { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback& cb)              // 消息到达时的回调
    { messageCallback_ = cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback& cb)  // 写操作完成时的回调
    { writeCompleteCallback_ = cb; }
    void setCloseCallback(const CloseCallback& cb)                  // 连接关闭时的回调
    { closeCallback_ = cb; }
    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark) // 发送缓冲区达到高水位时的回调
    { 
        highWaterMarkCallback_ = cb; 
        highWaterMark_ = highWaterMark; 
    }

    // 连接建立后 由 TcpServer 调用
    void connectEstablished();

    // 连接销毁前调用
    void connectDestroyed();


private:
    // 连接状态枚举
    enum StateE
    {
        kDisconnected, // 已经断开连接
        kConnecting,   // 正在连接
        kConnected,    // 已连接
        kDisconnecting,// 正在断开连接
    };

    // 修改连接状态
    void setState(StateE state) { state_ = state; }

    // 响应Channel中的事件
    void handleRead(Timestamp receiveTime); // 处理读事件
    void handleWrite(); // 处理写事件
    void handleClose(); // 处理连接关闭事件
    void handleError(); // 处理错误事件

    // 实际执行数据发送逻辑  在 loop_ 所在线程中调用
    void sendInLoop(const void* date, size_t len);
    void sendFileInLoop(int fileDescriptor, off_t offset, size_t count);

    // 真正执行关闭写端操作  在 loop_ 所在线程中调用
    void shutdownInLoop();
    
     
    EventLoop *loop_;           // 所属EventLoop  若为多Reactor 该loop_指向subloop 若为单Reactor 该loop_指向baseloop
    const std::string name_;    // 连接名称
    std::atomic_int state_;     // 连接状态 (StateE)
    bool reading_;              // 连接是否在监听读事件

    // Socket Channel 
    std::unique_ptr<Socket> socket_;    
    std::unique_ptr<Channel> channel_;

    const InetAddress localAddr_; // 服务端地址 (本地地址)
    const InetAddress peerAddr_;  // 客户端地址 (对端地址)

    // 这些回调TcpServer也有 由 TcpServer 传递并设置到 TcpConnection 中
    // 用户写入TcpServer注册 => TcpServer将注册的回调传给TcpConnection => TcpConnection再将回调注册到Channel中
    ConnectionCallback connectionCallback_;       // 连接建立/销毁的回调（通知用户连接已建立或销毁）
    MessageCallback messageCallback_;             // 有读写消息时的回调
    WriteCompleteCallback writeCompleteCallback_; // 消息发送完成以后的回调
    HighWaterMarkCallback highWaterMarkCallback_; // 高水位回调
    CloseCallback closeCallback_;                 // 关闭连接的回调（Channel关闭事件，通知TcpServer删除该连接）
    
    size_t highWaterMark_; // 高水位阈值 当输出缓冲区大小超过该值时会触发 highWaterMarkCallback_ 回调

    // 数据缓冲区
    Buffer inputBuffer_;  // 接受数据的缓冲区
    Buffer outputBuffer_; // 发送数据的缓冲区 用户send向outputBuffer_发

};