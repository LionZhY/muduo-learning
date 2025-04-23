#include <sys/eventfd.h> // 提供eventfd()系统调用，用于用户态线程/进程间事件通知
#include <unistd.h>      // 提供POSIX API 如close()
#include <fcntl.h>       // 提供文件描述符控制，如EFD_NONBLOCK等常量
#include <errno.h>
#include <memory>

#include "EventLoop.h"
#include "Logger.h"
#include "Channel.h"
#include "Poller.h"

// 每个线程都有独立的 t_loopInThisThread 指针  保证每个线程只拥有一个 EventLoop
__thread EventLoop* t_loopInThisThread = nullptr; 

// 定义默认的Poller IO复用接口的超时时间
const int kPollTimeMs = 10000; // 10000毫秒 = 10秒 用于 poll() / epoll_wait() 等函数中的超时参数



// 创建eventfd 用于线程间事件唤醒
int createEventfd()
{
   int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC); // 该fd被主线程或其他线程写入，用于唤醒当前线程中的 EventLoop
   if (evtfd < 0) // 创建失败
   {
       LOG_FATAL("eventfd error:%d\n", errno);
   }
   return evtfd;
}


// 构造和析构
EventLoop::EventLoop()
    : looping_(false)
    , quit_(false)
    , callingPendingFunctors_(false)
    , threadId_(CurrentThread::tid())           // 记录当前线程ID（仅允许本线程使用）
    , poller_(Poller::newDefaultPoller(this))   // 创建poller对象
    , wakeupFd_(createEventfd())                // 创建eventfd，用于跨线程唤醒当前EventLoop
    , wakeupChannel_(new Channel(this, wakeupFd_)) // 创建Channel，监听wakeFd_的读事件
{
    // 打印调试信息：当前EventLoop对象地址及其所在线程ID
    LOG_DEBUG("EventLoop created %p in thread %d\n", this, threadId_);
    
    // 如果当前线程已经绑定一个 EventLoop，打印错误并终止
    if (t_loopInThisThread)
    {
        LOG_FATAL("Another EventLoop %p exists in this thread %d\n", t_loopInThisThread, threadId_);
    }
    else // 否则将当前对象赋值给 t_loopInThisThread，标记当前线程已绑定该 EventLoop。
    {
        t_loopInThisThread = this;
    }

    // 设置wakeupChannel的读事件回调函数为 handleRead()，确保收到唤醒通知时能正确处理
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    
    // 启用 wakeupChannel_ 的读事件监听
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop()
{
    wakeupChannel_->disableAll(); // Channel取消监听所有事件
    wakeupChannel_->remove();     // 把Channel从EventLoop上删除掉
    ::close(wakeupFd_);           // 关闭eventfd
    t_loopInThisThread = nullptr; // 清空本线程的t_loopInThisThread指针
}


// 开启事件循环
void EventLoop::loop()
{
    looping_ = true; // 标记EventLoop正在运行主循环
    quit_ = false;   // 清除退出标志，以防该EventLoop被重复使用

    // 打印日志：该EventLoop正在事件循环中
    LOG_INFO("EventLoop %p start looping\n", this);

    // 循环执行 直到外部设置quit_ = true
    while (!quit_)
    {
        activeChannels_.clear(); // 清除上次poll()得到的活跃Channel
        
        // 等待事件
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_); // 调用poll(即epoll_wait)填充activeChannels_

        // 分发事件
        for (Channel* channel : activeChannels_) // 遍历所有活跃的Channel，调用其handleEvent()进行事件处理
        {
            // 处理事件
            channel->handleEvent(pollReturnTime_); 
        }

        /**
         * 执行当前EventLoop事件循环需要处理的回调操作
         * 对于线程数 >= 2的情况  IO线程mainloop(mainReactor)主要工作：
         * accept接受链接=>将accept返回的connfd打包为Channel => TcpServer::newConnection通过轮询将TcpConnection对象分配给subLoop处理
         * 
         * mainloop调用queueInLoop将回调加入subloop
         * 该回调需要subloop执行 但subloop还在poller_->poll处阻塞  queueInLoop通过wakeup将subloop唤醒
         */

        // 执行延迟提交的任务回调（queueInLoop() 或 runInLoop() 添加到 EventLoop 的回调函数）
        doPendingFunctors(); 
    }

    // quit_ = true 循环结束，打印结束日志
    LOG_INFO("EventLoop %p stop looping.\n", this);
    // 重置状态位，允许之后再次进入循环
    looping_ = false;
}

/**
 * 退出事件循环
 * 1. 如果loop在自己的线程中调用quit成功了，说明当前线程已经执行完了loop()函数的poller_->poll并退出
 * 2. 如果不是当前EventLoop所属线程中调用quit退出EventLoop 需要唤醒EventLoop所属线程的epoll_wait
 * 
 * 比如在一个subloop(worker)中调用mainloop(IO)的quit时 需要唤醒mainloop(IO)的poller_->poll 让其执行完loop()函数
 * 
 * ！！！ 注意： 正常情况下 mainloop负责请求连接 将回调写入subloop中 通过生产者消费者模型即可实现线程安全的队列
 * ！！！       但是muduo通过wakeup()机制 使用eventfd创建的wakeupFd_ notify 使得mainloop和subloop之间能够进行通信
 */


// 退出事件循环
void EventLoop::quit()
{
    quit_ = true; // 在 EventLoop::loop() 的循环条件中被检查

    // 判断当前线程是否是这个EventLoop所属的线程
    if (!isInLoopThread())
    {
        wakeup(); // 如果不是，即其他线程在调用quit(),当前EventLoop可能在阻塞，需要唤醒poll
    }
}


// 若是当前loop线程创建的EventLoop，立即执行cb
void EventLoop::runInLoop(Functor cb)
{
    if (isInLoopThread()) // 如果当前线程是否是这个EventLoop所属的线程，直接执行回调cb()
    {
        cb();
    }
    else // 非EventLoop所属线程调用的，就要唤醒EventLoop所在线程执行cb
    {
        queueInLoop(cb); // 加入待执行回调队列pendingFunctors_
    }
}


// 把上层注册的回调函数cb放入队列中，唤醒loop所在的线程执行cb
void EventLoop::queueInLoop(Functor cb)
{
    {
        std::unique_lock<std::mutex> lock(mutex_); // 加锁保护pendingFucntors_
        pendingFunctors_.emplace_back(cb); // 将传入的回调函数加入到 pendingFunctors_ 尾部
    }
    /**
    * || callingPendingFunctors的意思是 
    * 当前loop正在执行回调中 但是loop的pendingFunctors_中又加入了新的回调 
    * 需要通过wakeup写事件 唤醒相应的需要执行上面回调操作的loop的线程 
    * 让loop()下一次poller_->poll()不再阻塞（阻塞的话会延迟前一次新加入的回调的执行），
    * 然后继续执行pendingFunctors_中的回调函数
    **/

    if(!isInLoopThread() || callingPendingFunctors_)
    {
        wakeup(); // 唤醒loop所在线程
    }
}


// 给eventfd返回的文件描述符wakeupFd_绑定的事件回调 
void EventLoop::handleRead()
{
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof(one)); // 读出 eventfd 数据，防止 epoll 重复触发
    if (n != sizeof(one))
    {
        LOG_ERROR("EventLoop::handleRead() reads %lu bytes instead of 8\n", n);
    }
}


// 通过eventfd唤醒loop所在的线程  向wakeupFd_写一个数据 wakeupChannel就发生读事件 当前loop线程就会被唤醒
void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof(one));
    if (n != sizeof(one))
    {
        LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8\n", n);
    }
}


// EventLoop的方法 => Poller的方法
void EventLoop::updateChannel(Channel* channel)
{
    poller_->updateChannel(channel);
}
void EventLoop::removeChannel(Channel* channel)
{
    poller_->removeChannel(channel);
}
bool EventLoop::hasChannel(Channel* channel)
{
    return poller_->hasChannel(channel);
}



// 执行上层回调  执行所有延迟提交的任务回调
void EventLoop::doPendingFunctors()
{
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_); // 交换的方式减少了锁的临界区范围 提升效率 同时避免了死锁 
                                         // 如果执行functor()在临界区内 且functor()中调用queueInLoop()就会产生死锁
    }

    for (const Functor &functor : functors)
    {
        functor(); // 执行当前loop需要执行的回调操作
    }

    callingPendingFunctors_ = false;

}