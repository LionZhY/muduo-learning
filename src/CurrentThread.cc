#include "CurrentThread.h"

/**缓存tid，避免频繁调用syscall，减少系统调用开销
 * 每个线程都有独立的t_cacheTid，不会影响其他线程（使用__thread关键字）
 * 
 * 线程刚创建时，t_cacheTid = 0，tid还未缓存
 * 首次调用tid(), 进入if语句，执行cacheTid() --> syscall() 获取tid，并缓存到t_cacheTid
 * 后续调用tid()，直接返回缓存的t_cacheTid，不再调用cacheTid() --> syscall
 */


namespace CurrentThread
{
    // t_cacheTid  初始值0
    __thread int t_cachedTid = 0;
 

    // 实现cacheTid()  获取当前线程的真是线程ID（tid），并缓存到线程局部变量t_cachedTid中
    void cacheTid()
    {
        if (t_cachedTid == 0) // 只有t_cacheTid未赋值（即tid()第一次调用）才执行
        {
            t_cachedTid = static_cast<pid_t>(::syscall(SYS_gettid)); // 执行syscall，将tid缓存到t_cacheTid
            
            // syscall(SYS_gettid) Linux提供的系统调用，返回当前线程的tid（线程ID）
            // static_cast<pid_t>() 类型转换，确保返回值为pid_t
        }
    }

}
