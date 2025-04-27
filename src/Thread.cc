#include "Thread.h"
#include "CurrentThread.h"

#include <semaphore.h> // 使用信号量sem_t确保主线程能正确获取新创建线程的tid


std::atomic_int Thread::numCreated_(0); // 记录线程数，初始化为0（static成员变量 类内声明 类外定义）


// 构造
Thread::Thread(ThreadFunc func, const std::string &name) // 接收线程回调函数func，和线程名称name
    : started_(false)           // 初始状态：线程未启动
    , joined_(false)            // 初始状态：线程未join
    , tid_(0)                   // 初始tid = 0，等待start()后获取
    , func_(std::move(func))    // 线程执行的函数（move转移资源所有权）
    , name_(name)               // 线程名称
{
    setDefaultName(); // 如果name为空，就设置默认名称
}

// 析构
Thread::~Thread()
{
    if (started_ && !joined_) // 如果线程已启动，但未join()，则调用detach()
    {
        thread_->detach(); // detach()把线程设置为分离状态，不用主线程等它，线程运行后自动销毁（非阻塞）
    }
}


// 创建线程
// start() 创建一个新线程, 并去执行用户传进来的回调函数func_
void Thread::start() 
{
    started_ = true; // 标记线程已启动
    
    sem_t sem;                // 声明sem_t信号计数量
    sem_init(&sem, false, 0); // 初始化信号量sem值为0，false指的是只线程间共享，不设置进程间共享
    
    // 用一个 shared_ptr （thread_）来管理线程对象，lambda 表达式作为线程体
    thread_ = std::shared_ptr<std::thread> (new std::thread ( [&]() { 
        tid_ = CurrentThread::tid(); // 获取线程的tid值
        sem_post(&sem);              // 释放信号量，通知主线程tid_已准备好
        func_();                     // 执行传入的回调函数
        }
    ));

    // 等待信号量>0，即子线程调用了 sem_post()
    sem_wait(&sem);
}



// 阻塞当前线程
void Thread::join() // 确保主线程等待子线程结束，否则子线程可能在Thread析构后仍然运行，导致悬空线程
{
    joined_ = true;  // 标记线程已join
    thread_->join(); // 阻塞等待线程结束
}


// 设置默认线程名称
void Thread::setDefaultName()
{
    int num = ++numCreated_; // 递增线程计数
    if (name_.empty()) // 如果name为空，则自动分配名称
    {
        char buf[32] = {0};
        snprintf(buf, sizeof buf, "Thread%d", num); // 自动命名为 "Thread1""Thread2"...
        name_ = buf;
    }
}