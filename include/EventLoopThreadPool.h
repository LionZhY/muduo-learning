#pragma once

#include <functional>
#include <string>
#include <vector>
#include <memory>

#include "noncopyable.h"
class EventLoop;
class EventLoopThread;

class EventLoopThreadPool : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    // 构造和析构
    EventLoopThreadPool(EventLoop* baseLoop, const std::string &nameArg); // 传入主线程baseLoop, 线程池名称
    ~EventLoopThreadPool();

    // 设置线程池中EventLoopThread的数量
    void setThreadNum(int numThreads) { numThreads_ = numThreads; }

    // 启动线程池
    void start(const ThreadInitCallback& cb = ThreadInitCallback());

    // 采用轮询策略返回一个 EventLoop*
    EventLoop* getNextLoop(); // 如果工作在多线程中，baseLoop_(mainLoop)会默认以轮询的方式分配Channel给subLoop

    // 获取所有的EventLoop
    std::vector<EventLoop*> getAllLoops(); 

    // 查询线程池是否已启动
    bool started() const { return started_; }

    // 返回线程池名称
    const std::string name() const { return name_; } 


private:
    EventLoop* baseLoop_; // 指向主线程的EventLoop，在只有一个线程时直接用这个处理所有IO
    std::string name_;   // 线程池名称，通常由用户指定，线程池中EventLoopThread名称依赖于线程池名称

    bool started_;   // 线程池是否已经启动
    int numThreads_; // 线程池中线程的数量
    int next_;       // 轮询索引，新链接到来，所选择的EventLoop的索引

    std::vector<std::unique_ptr<EventLoopThread>> threads_; // IO线程列表 保存所有 EventLoopThread 对象
    std::vector<EventLoop*> loops_; // 线程池中EventLoop的列表，指向的是EventLoopThread线程函数创建的EventLoop对象

};