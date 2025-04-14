#include "Poller.h"
#include "Channel.h"

// 构造
Poller::Poller(EventLoop* loop)
    : ownerLoop_(loop) // 初始化列表将 ownerLoop_ 成员设置为 loop, 表示这个 Poller 实例属于哪个事件循环对象
{

}


// 判断一个 Channel 是否已经被添加到 Poller 的监听列表中
bool Poller::hasChannel(Channel* channel) const
{
    auto it = channels_.find(channel->fd()); // 得到channel的fd，以fd为key在channels_查找
    
    return it != channels_.end() && it->second == channel; 
    
    // it != channels_.end() 找到，说明这个fd已经注册到poller
    // 还要检查it->second 也就是channel是否就是这个对象本身
}