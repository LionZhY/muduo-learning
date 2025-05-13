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


class TcpConnection : noncopyable, public std::enable_shared_from_this<TcpConnection>
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
    // 获取地址
    const InetAddress& localAddress() const { return localAddr_; }
    const InetAddress& peerAddress() const { return peerAddr_; }
    // 获取连接状态
    bool connected() const { return state_ == kConnected; }

    // 发送数据
    void send(const std::string& buf);
    void sendFile(int fileDescriptor, off_t offset, size_t count);

    // 主动关闭连接（半关闭连接）
    void shutdown();

    // 用户设置回调
    void setConnectionCallback(const ConnectionCallback& cb)        // 连接建立/断开      
    { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback& cb)              // 接收消息
    { messageCallback_ = cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback& cb)  // 发送完成
    { writeCompleteCallback_ = cb; }
    void setCloseCallback(const CloseCallback& cb)                  // 高水位回调
    { closeCallback_ = cb; }
    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark) 
    { highWaterMarkCallback_ = cb; highWaterMark_ = highWaterMark; }// 连接关闭时

    // 连接建立后调用
    void connectEstablished();

    // 连接销毁前调用
    void connectDestroyed();


private:
    // 连接状态
    enum StateE
    {
        kDisconnected, // 已经断开连接
        kConnecting,   // 正在连接
        kConnected,    // 已连接
        kDisconnecting,// 正在断开连接
    };

    void setState(StateE state) { state_ = state; }

    // 响应Channel中的事件
    void handleRead(Timestamp receiveTime); // 处理读事件
    void handleWrite(); // 处理写事件
    void handleClose(); // 处理连接关闭事件
    void handleError(); // 处理错误事件

    // 在EventLoop中发送数据
    void sendInLoop(const void* date, size_t len);
    void sendFileInLoop(int fileDescriptor, off_t offset, size_t count);
    // 在EventLoop中关闭连接
    void shutdownInLoop();
    
     
    EventLoop *loop_; // 所属EventLoop  若为多Reactor 该loop_指向subloop 若为单Reactor 该loop_指向baseloop
    const std::string name_;    // 连接名称
    std::atomic_int state_;     // 连接状态 (StateE)
    bool reading_;              // 连接是否在监听读事件

    // Socket Channel 这里和Acceptor类似  Acceptor => mainLoop   TcpConnection => subLoop
    std::unique_ptr<Socket> socket_;    
    std::unique_ptr<Channel> channel_;

    const InetAddress localAddr_; // 服务端地址 (本地地址)
    const InetAddress peerAddr_;  // 客户端地址

    // 这些回调TcpServer也有 
    // 用户通过写入TcpServer注册 TcpServer再将注册的回调传给TcpConnection  TcpConnection再将回调注册到Channel中
    ConnectionCallback connectionCallback_;       // 有新连接时的回调
    MessageCallback messageCallback_;             // 有读写消息时的回调
    WriteCompleteCallback writeCompleteCallback_; // 消息发送完成以后的回调
    HighWaterMarkCallback highWaterMarkCallback_; // 高水位回调
    CloseCallback closeCallback_;                 // 关闭连接的回调
    
    size_t highWaterMark_; // 高水位阈值

    // 数据缓冲区
    Buffer inputBuffer_;  // 接受数据的缓冲区
    Buffer outputBuffer_; // 发送数据的缓冲区 用户send向outputBuffer_发

};