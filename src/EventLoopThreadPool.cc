#include <memory>

#include "EventLoopThread.h"
#include "EventLoopThreadPool.h"
#include "Logger.h"



// 构造和析构
EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseLoop, const std::string &nameArg)
    : baseLoop_(baseLoop)   // 主线程 EventLoop 指针
    , name_(nameArg)        // 线程池名称
    , started_(false)       // 线程池启动标志
    , numThreads_(0)        // 线程池中的线程数
    , next_(0)              // 轮询索引
{

}

EventLoopThreadPool::~EventLoopThreadPool()
{
    // Don't delete loop, it's stack variable
    /**
     * baseLoop_ 是用户在主程序栈上创建的对象，不能由线程池销毁，否则会引发悬垂指针或双重释放问题。
     * 子线程的 EventLoopThread 已由 unique_ptr 管理，会自动析构，无需手动管理。
     */
}


// 启动线程池
void EventLoopThreadPool::start(const ThreadInitCallback& cb)
{
    started_ = true;
    // 遍历需要创建的线程数量，依次初始化每一个 EventLoopThread
    for (int i = 0; i < numThreads_; ++i)
    {
        char buf[name_.size() + 32];
        snprintf(buf, sizeof buf, "%s%d", name_.c_str(), i); // 格式化生产每个子线程的名称

        EventLoopThread* t = new EventLoopThread(cb, buf);       // 创建新的EventLoopThread对象，传入初始化回调cb
        threads_.push_back(std::unique_ptr<EventLoopThread>(t)); // 原始指针t封装成unique_ptr，放入threads_

        loops_.push_back(t->startLoop()); // startLoop()内部启动新线程，在新线程中创建新的EventLoop，返回的EventLoop*放入loops_
    }

    // 如果 numThreads_ == 0，说明没有创建任何子线程（单线程模式），且用户提供了初始化回调 cb
    if (numThreads_ == 0 && cb) // 整个服务端只有一个线程运行baseLoop
    {
        cb(baseLoop_); // 直接在主线程的 baseLoop_ 上调用一次初始化。
    }
}

// 采用轮询策略获取下一个 EventLoop*
EventLoop* EventLoopThreadPool::getNextLoop() // 如果工作在多线程中，baseLoop_(mainLoop)会默认以轮询的方式分配Channel给subLoop
{
    /**
     * 如果只是设置一个线程 也就是只有一个mainReactor 无subReactor
     * 那么轮询只有一个线程 getNextLoop()每次都返回当前的baseLoop_
     */

    EventLoop* loop = baseLoop_; // 默认让loop指向主线程的baseLoop_ 如果后面发现有子线程，才改用子线程的 EventLoop

    /**
     * 通过轮询获取下一个处理事件的loop
     * 如果没设置多线程数量，则不会进去，相当于直接返回baseLoop
     */
    
    // 如果 loops_ 非空（即线程池中有子线程）
    if (!loops_.empty())
    {
        loop = loops_[next_]; // 使用当前next_索引选取EventLoop
        ++next_;              // next_自增，指向下一个

        // 轮询调度使得每个 EventLoop 负载尽可能均匀，不会让单个 EventLoop 过载
        if (next_ >= loops_.size())
        {
            next_ = 0;
        }
    }

    return loop; // 返回选中的EventLoop*
}


// 获取所有的EventLoop
std::vector<EventLoop*> EventLoopThreadPool::getAllLoops()
{
    // 如果 loops_ 为空（说明线程池未启用多线程），则返回一个仅包含 baseLoop_ 的单元素向量
    if (loops_.empty()) 
    {
        return std::vector<EventLoop*>(1, baseLoop_);
    }
    else // 如果有子线程，直接返回保存了所有子线程 EventLoop* 的 loops_ 容器
    {
        return loops_;
    }
}