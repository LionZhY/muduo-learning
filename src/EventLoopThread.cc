#include "EventLoopThread.h"
#include "EventLoop.h"

// 构造和析构
EventLoopThread::EventLoopThread(const ThreadInitCallback &cb, 
                                 const std::string &name)
    : loop_(nullptr)
    , exiting_(false)
    , thread_(std::bind(&EventLoopThread::threadFunc, this), name) // 绑定线程函数threadFunc，和线程名称name
    , mutex_()
    , cond_()
    , callback_(cb) // 保存传入的线程初始化回调函数
{

}

EventLoopThread::~EventLoopThread()
{
    exiting_ = true; // 标记当前对象处于退出阶段

    if (loop_ != nullptr) // 如果loop_已创建（即子线程中EventLoop已存在）
    {
        loop_->quit();  // 退出事件循环
        thread_.join(); // 等待子线程thread_结束，防止主线程提前销毁资源
    }
}

// 启动内部线程
EventLoop* EventLoopThread::startLoop()
{
    thread_.start(); //启用底层线程Thread类对象thread_中通过start()创建的线程

    EventLoop* loop = nullptr; // 临时指针，保存创建好的EventLoop对象

    {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this](){return loop_ != nullptr;}); // 条件遍历等待，知道loop_被子线程设置为非空
        loop = loop_;
    }

    return loop; // 返回已经创建并初始化完成的EventLoop对象指针
}


// 线程函数 是在单独的新线程里运行的 负责创建EventLoop对象并进入事件循环
void EventLoopThread::threadFunc()
{
    EventLoop loop; // 创建一个独立的EventLoop对象，和上面的线程是一一对应的 one loop per thread

    // 如果存在线程初始化回调，执行该回调，并传入当前EventLoop指针
    if (callback_) 
    {
        callback_(&loop);
    }

    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop; // 将局部EventLoop对象的地址赋值给成员遍历loop_
        cond_.notify_one(); // 唤醒主线程 通知EventLoop已创建完成
    }

    loop.loop(); // 执行EventLoop的loop() 开启了底层的Poller的poll()
    
    std::unique_lock<std::mutex> lock(mutex_); // 事件循环退出后，加锁准备清理
    loop_ = nullptr; // 将loop_指针清空，防止悬挂指针
}