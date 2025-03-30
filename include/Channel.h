#pragma once // 确保头文件只被包含一次

#include <functional> // 引入函数对象相关的标准库，用于存储回调函数std::function
#include <memory>     // 引入智能指针相关标准库，用于shared_ptr和weak_ptr绑定机制

#include "noncopyable.h" // 继承noncopyable类，禁止拷贝构造和赋值构造
#include "Timestamp.h"   // 引入时间戳类Timestamp，用于记录事件发生时间

class EventLoop; // EventLoop类的前向声明
                 // EventLoop是Channel依赖的类，在EventLoop中定义，
                 // 这里用前向声明，减少头文件依赖

                 
/**
 * 理清楚 EventLoop、Channel、Poller之间的关系  Reactor模型上对应多路事件分发器
 * Channel理解为通道 封装了sockfd和其感兴趣的event 如EPOLLIN、EPOLLOUT事件 还绑定了poller返回的具体事件
 **/


 // Channel 代表一个 I/O 事件通道，管理 文件描述符（fd） 和 事件回调

class Channel : noncopyable // 继承noncopyable类，避免对象被拷贝
{
public:
    
    // 定义普通事件的回调类型，表示无参数、无返回值的函数
    using EventCallback = std::function<void()>; // 一般事件的回调（如写事件、关闭事件）
    // 定义读事件的回调类型，回调函数带有时间戳参数
    using ReadEventCallback = std::function<void(Timestamp)>; 

    
    // 构造和析构
    // EventLoop *loop：指向所属的事件循环，Channel 依赖 EventLoop 进行事件管理
    Channel(EventLoop *loop, int fd); 
    ~Channel();


    // 事件处理
    // fd得到Poller通知后，处理事件 handleEvent在EventLoop::loop()中调用
    void handleEvent(Timestamp receiveTime);

    
    // 回调函数
    // 设置不同事件的回调函数，通过 std::function 绑定：向Channel对象注册各类事件
    void setReadCallback  (ReadEventCallback cb) { readCallback_  = std::move(cb); } // 可读事件
    void setWriteCallback (EventCallback cb)     { writeCallback_ = std::move(cb); } // 可写事件
    void setcloseCallback (EventCallback cb)     { closeCallback_ = std::move(cb); } // 连接关闭
    void setErrorCallback (EventCallback cb)     { errorCallback_ = std::move(cb); } // 错误处理


    // 绑定对象生命周期
    void tie(const std::shared_ptr<void> &);// 绑定对象声明周期，tie_绑定shared_ptr
                                            // 防止Channel被手动remove后，仍在执行回调，避免访问悬空指针

    // 获取信息
    int fd() const { return fd_; }                  // fd() 返回监听的文件描述符（socket）
    int events() const {return events_; }           // events() 返回当前感兴趣的时间（EPOLLIN、EPOLLOUT）
    void set_revents(int revt) { revents_ = revt; } // set_revents() 设置Poller返回的实际事件


    // epoll事件控制，设置fd相应的事件状态，相当于epoll_ctl add delete
    void enableReading()  { events_ |= kReadEvent;    update(); } // 监听EPOLLIN事件（可读）
    void disableReading() { events_ &= ~kReadEvent;   update(); } // 取消监听可读
    void enableWriting()  { events_ |= kWriteEvent;   update(); } // 监听EPOLLOUT事件（可写）
    void disableWriting() { events_ &= ~kWriteEvent;  update(); } // 取消监听可写
    void disableAll()     { events_ = kNoneEvent;     update(); } // 取消监听所有事件


    // 返回fd当前的事件状态
    bool isNoneEvent() const { return events_ == kNoneEvent; } // 当前是否无事件监听
    bool isWriting() const   { return events_ & kWriteEvent; } // 是否监听写事件
    bool isReading() const   { return events_ & kReadEvent; }  // 是否监听读事件


    // Poller内部索引管理
    int index() { return index_; } // index_在Poller里用于标记Channel
    void set_index(int idx) { index_ = idx; };


    // one loop per thread
    EventLoop* ownerLoop() { return loop_; } // ownerLoop() 返回所属的EventLoop，确保channel只能在创建它的EventLoop线程里操作
    
    void remove(); // 用于从Poller中移除channel



private:
    // 通知 Poller 更新 fd 的监听状态
    void update(); // 由EventLoop调用

    void handleEventWithGuard(Timestamp receiveTime); // 进一步封装handleEvent，用于tie_绑定的安全检查，
                                                    // 确保Channel还存活时才执行回调

    // 事件类型
    static const int kNoneEvent;    // 不监听任何事件
    static const int kReadEvent;    // 监听EPOLLIN（可读）
    static const int kWriteEvent;   // 监听EPOLLOUT（可写）

    
    EventLoop *loop_;   // 所属的EventLoop
    const int fd_;      // 监听的文件描述符

    int events_;        // 关心的事件(如EPOLLIN EPOLLOUT)
    int revents_;       // 实际发生的事件（Poller返回的实际事件）

    int index_;         // Poller内部标识

    // 生命周期管理
    std::weak_ptr<void> tie_; // 绑定的shared_ptr，防止对象提前销毁
    bool tied_;               // 标记tie_是否已绑定


    // 各事件类型发生时的处理函数
    // 因为Channel通道里可获知fd最终发生的具体的事件events，所以它负责调用具体事件的回调操作
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
    
 };