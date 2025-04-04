#pragma once

#include <unistd.h>         // 是UNIX 系统 API，提供进程和线程管理相关函数 提供gitpid() syscall()
#include <sys/syscall.h>    // 提供SYS_gettid  获取线程ID (tid)


/* 获取当前线程ID tid() --> 第一次获取tid，执行cacheTid() --> return t_cachedTid*/


// CurrentThread 命名空间  用于封装当前线程的相关信息
namespace CurrentThread
{
    // 线程局部变量的外部声明
    extern __thread int t_cachedTid; // 缓存当前线程ID（tid），因为系统调用非常耗时，拿到tid后将其保存


    // cacheTid() 
    void cacheTid(); // 在tid()首次调用时执行，用于获取tid并缓存到t_cachedTid


    // tid() 获取当前线程的真实线程ID（tid）
    inline int tid() // 内联函数 可以直接在头文件中定义
    {
        if (__builtin_expect(t_cachedTid == 0, 0)) 
        {
            // __builtin_expect是一种底层优化，表示t_cachedTid = 0，即如果还未获取tid，进入if，执行cacheTid()
            cacheTid(); 
        }
        return t_cachedTid;
    }

    
    

}