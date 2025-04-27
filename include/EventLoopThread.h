#pragma once 

#include <functional>
#include <mutex>
#include <condition_variable> // 引入std::condition_variable，用于线程间同步等待
#include <string>

#include "noncopyable.h"
#include "Thread.h"

class EventLoop;

class EventLoopThread : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>; // 类型别名 以EventLoop*为参数，无返回值的可调用对象

    // 构造和析构
    EventLoopThread(const ThreadInitCallback &cb = ThreadInitCallback(), // 初始化回调，默认为空回调
                    const std::string &name = std::string()); // 线程名称，默认为空字符串
    
    ~EventLoopThread();

    // 启动内部线程
    EventLoop* startLoop();

private:
    // 线程函数 是在单独的新线程里运行的 负责创建EventLoop对象并进入事件循环
    void threadFunc();

    EventLoop* loop_; // 指向新线程中创建的EventLoop对象，该对象只存在于子线程中，主线程通过startLoop()获取
    bool exiting_;    // 指示析构阶段是否已进入资源清理流程
    Thread thread_;   // 封装的线程对象，负责创建运行EventLoop的线程
    
    std::mutex mutex_; // 互斥锁
    std::condition_variable cond_; // 条件变量 配合mutex_使用，用于通知主线程 EventLoop已创建完毕

    ThreadInitCallback callback_; // 用户传入的线程初始化回调

};