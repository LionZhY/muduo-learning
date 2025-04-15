#include <stdlib.h>

#include "Poller.h"
#include "EPollPoller.h"

/**
 * Poller.h 里声明
 * 根据环境变量选择合适的 I/O 复用机制（如 poll 或 epoll），并创建相应的 Poller 实例
 */

// 创建一个默认的 Poller 子类对象，返回对象指针
Poller* Poller::newDefaultPoller(EventLoop* loop) 
{
    if (::getenv("MUDUO_USE_POLL")) // 查询环境变量 MUDUO_USE_POLL 是否被设置
    {
        return nullptr; // 生成poll实例
    }
    else
    {
        return new EPollPoller(loop); // 生成epoll的实例(默认)
    }

}