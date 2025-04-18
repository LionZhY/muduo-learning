#pragma once 

#include <vector>
#include <sys/epoll.h>

#include "Poller.h"
#include "Timestamp.h"


/**
 * epoll的使用:
 * 1. epoll_create
 * 2. epoll_ctl (add, mod, del)
 * 3. epoll_wait
 **/

class Channel;

class EPollPoller : public Poller // 公共继承 Poller
{
public:
    // 构造 
    EPollPoller(EventLoop* loop); // 参数是指向该 EPollPoller 所属的 EventLoop 的指针

    // 虚析构 （重写父类的虚析构）
    ~EPollPoller() override;


    // 重写基类Poller的（纯虚函数）方法  poll()  updateChannel()  removeChannel()
    Timestamp poll(int timeoutMs, ChannelList* activeChannels) override;
    void updateChannel(Channel* channel) override;
    void removeChannel(Channel* channel) override;


private:

    // 填写活跃的连接channel到activeChannels
    void fillActiveChannels(int numEvents, ChannelList* activeChannels) const;

    // 更新channel，其实就是调用epoll_ctl
    void update(int operation, Channel* channel);


    
    int epollfd_;       // 用于标识epoll实例   epoll_creat 创建返回的fd保存在epollfd_中

    static const int kInitEventListSize = 16; // 初始化epoll事件列表events_(vector<epoll_event>)的容量为16
    
    using EventList = std::vector<epoll_event>; // 容器类型别名  epoll事件列表
    EventList events_;  // 存放epoll_wait得到的事件列表，初始容量为16

};