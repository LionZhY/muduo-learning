#include "Poller.h"
#include "Channel.h"

// 构造
Poller::Poller(EventLoop* loop)
    : ownerLoop_(loop) // 初始化列表将 ownerLoop_ 成员设置为 loop, 表示这个 Poller 实例属于哪个事件循环对象
{

}


// 判断参数channel是否在当前的poller中
bool Poller::hasChannel(Channel* channel) const
{
    auto it = channels_.find(channel->fd()); 
    return it != channels_.end() && it->second == channel;

}