#pragma once 

#include <functional>
#include <vector>
#include <atomic> // 用于线程安全的原子变量
#include <memory>
#include <mutex>  // 互斥锁，用于保护临界资源

#include "noncopyable.h"
#include "Timestamp.h"                                       
#include "CurrentThread.h"

class Channel;
class Poller;


/**
 * 事件循环类
 * 主要包含了两个大模块：Channel  Poller(epoll的抽象)
 */


class EventLoop : noncopyable
{
public:
    using Functor = std::function<void()>; // 回调类型别名

    // 构造和析构
    EventLoop();
    ~EventLoop();

    // 开启事件循环
    void loop();
    // 退出事件循环
    void quit();


    // 返回上一次 poller_->poll()的返回时间
    Timestamp pollReturnTime () const   { return pollReturnTime_; }

    // 在当前loop中执行回调
    void runInLoop(Functor cb);
    // 把上层注册的回调函数cb放入队列中，唤醒loop所在的线程执行cb
    void queueInLoop(Functor cb);

    // 通过eventfd唤醒loop所在的线程
    void wakeup();

    // EventLoop的方法 => Poller的方法
    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);
    bool hasChannel(Channel* channel);

    // 判断EventLoop对象是否在自己的线程里创建的
    // threadId_为EventLoop创建时的线程id CurrentThread::tid()为当前线程id
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }


private:
    // 给eventfd返回的文件描述符wakeupFd_绑定的事件回调 
    void handleRead(); // 当wakeup()时 即有事件发生 调用handleRead()读wakeupFd_的8字节 同时唤醒阻塞的epoll_wait

    // 执行上层回调
    void doPendingFunctors();


    using ChannelList = std::vector<Channel*>; // 当前活动的事件channel集合

    std::atomic_bool looping_;      // 事件循环是否正在进行 原子操作 底层通过CAS实现
    std::atomic_bool quit_;         // 标识退出loop循环

    const pid_t threadId_;          // 记录当前EventLoop是被哪个线程id创建的（标识当前EventLoop的所属线程id）

    Timestamp pollReturnTime_;      // Poller返回发生事件的Channels的时间点
    std::unique_ptr<Poller> poller_;// 使用 RAII 管理的 IO 多路复用器

    int wakeupFd_; // 当mainLoop获取一个新用户的Channel，需通过轮询算法选择一个subLoop，通过该成员唤醒subLoop处理Channel
    std::unique_ptr<Channel> wakeupChannel_; // 封装 wakeupFd_ 并监听其可读事件

    ChannelList activeChannels_; // 当前活跃的Channel集合

    std::atomic_bool callingPendingFunctors_; // 标识当前loop是否有需要执行的回调操作
    std::vector<Functor> pendingFunctors_;    // 存储loop待执行回调的队列
    
    std::mutex mutex_; // 互斥锁 用来保护上面vector容器的线程安全操作

};