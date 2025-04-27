#include "EventLoopThread.h"
#include "EventLoop.h"

// 构造和析构
EventLoopThread::EventLoopThread(const ThreadInitCallback &cb, // 线程启动时的初始化回调
                                 const std::string &name)      // 线程名称
    : loop_(nullptr)    // 还没有创建对应的EventLoop对象
    , exiting_(false)   // 目前线程不处于退出状态
    , thread_(std::bind(&EventLoopThread::threadFunc, this), name) // 绑定线程函数threadFunc，和线程名称name
    , mutex_()
    , cond_()
    , callback_(cb) // 初始化回调
{

}

EventLoopThread::~EventLoopThread()
{
    exiting_ = true; // 标记当前对象处于退出阶段

    // 只有在子线程中 EventLoop 对象已经创建完成，才进行后续的退出操作
    if (loop_ != nullptr) // 如果loop_已创建（即子线程中EventLoop已存在）
    {
        loop_->quit();  // 退出事件循环
        thread_.join(); // 主线程等待子线程thread_结束，防止主线程提前销毁资源
    }
}

// 启动内部线程
EventLoop* EventLoopThread::startLoop()
{
    thread_.start(); // 开启子线程，并执行回调threadFunc()

    EventLoop* loop = nullptr; // 临时指针，保存创建好的EventLoop对象

    // 加锁 主线程和子线程会同时访问 loop_，所以必须加锁防止数据竞争
    {
        std::unique_lock<std::mutex> lock(mutex_);
        // 主线程挂起等待，直到子线程创建好EventLoop，并设置好loop_指针
        cond_.wait(lock, [this](){return loop_ != nullptr;}); 
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