#pragma once

#include <functional>
#include "noncopyable.h"
#include "Socket.h"
#include "Channel.h"

class EventLoop;
class InetAddress;

class Acceptor : noncopyable
{
public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress&)>;

    // 构造  
    Acceptor(EventLoop* loop, // 指向主mainLoop
             const InetAddress& listenAddr, // 指定监听地址，封装IP和端口的地址对象
             bool reuseport); // 是否设置SO_REUSEPORT
             
    ~Acceptor();

    // 设置新连接的回调函数
    void setNewConnectionCallback(const NewConnectionCallback& cb) { NewConnectionCallback_ = cb; }

    // 判断是否在监听
    bool listenning() const { return listenning_; }

    // 监听本地端口
    void listen();


private:
    // 处理新用户的连接事件
    void handleRead(); 

    EventLoop* loop_; // Acceptor用的就是用户定义的那个baseLoop，也就是mainLoop

    Socket acceptSocket_;   // 专门用于接收新连接的socket (listen socket)
    Channel acceptChannel_; // 专门用于监听新连接的channel (与 acceptSocket 绑定的 Channel)

    NewConnectionCallback NewConnectionCallback_; // 新连接的回调函数，由外部设置

    bool listenning_; // 是否在监听

};