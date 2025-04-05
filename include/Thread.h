#pragma once

#include <functional>   // 用于存储可调用对象（线程函数）
#include <thread>       // 线程库
#include <memory>       // shared_ptr智能指针
#include <unistd.h>     // 提供pif_t相关函数
#include <string>       
#include <atomic>       // atomic_int 提供原子操作，保证线程安全

#include "noncopyable.h"

class Thread : noncopyable
{
public:
    
    using ThreadFunc = std::function<void()>;// ThreadFunc 作为std::function<void()>类型的别名

    // 构造和析构
    explicit Thread(ThreadFunc, const std::string &name = std::string());// 接收线程回调函数func，和线程名称name
    ~Thread();

    // 线程控制
    void start(); // 启动线程
    void join();  // 阻塞当前线程


    // started() 返回线程是否启动
    bool started()  { return started_; } 

    // tid() 返回线程ID（在start时分配）
    pid_t tid() const   { return tid_; }     

    // name() 返回线程名称
    const std::string &name() const { return name_; }    

    // numCreated() 获取当前已创建的线程总数
    static int numCreated() {return numCreated_;}


private:
    // 设置默认线程名称
    void setDefaultName();

    bool started_; // 线程是否已启动（默认false，start()之后true）
    bool joined_;  // 线程是否已join


    pid_t tid_;        // 线程ID，在start()里获取，在线程创建时再绑定
    std::string name_; // 线程名称

    ThreadFunc func_;  // 线程回调函数，要执行的函数

    std::shared_ptr<std::thread> thread_; // thred类型线程对象（使用shared_ptr，避免手动管理）


    static std::atomic_int numCreated_; // 原子类型，安全记录全局创建的线程数，每创建一个Thread，计数+1
                                        // 静态成员变量，需要类内声明，类外定义

};