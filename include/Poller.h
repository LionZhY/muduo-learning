#pragma once

#include <vector>
#include <unordered_map>

#include "noncopyable.h"
#include "Timestamp.h"


class Channel;
class EventLoop;

// muduo库中多路事件分发器的核心IO复用模块
class Poller
{
public:
    
    using ChannelList = std::vector<Channel*>; // 定义类型别名 表示活跃通道列表

    // 构造 
    Poller(EventLoop* loop); // 参数是该 Poller 所属的 EventLoop

    // 虚析构
    virtual ~Poller() = default; 


    // 给所有IO复用保留同一接口    (纯虚函数)

    // 核心IO复用接口  等待事件发生
    virtual Timestamp poll(int timeoutMs, ChannelList *activeChannel) = 0; 
    // 添加或更新一个Channel的事件监听状态
    virtual void updateChannel(Channel* channel) = 0;  
    // 从poller中移除不再需要监听的channel                    
    virtual void removeChannel(Channel* Channel) = 0;                      


    // 判断一个 Channel 是否已经被添加到 Poller 的监听列表中
    bool hasChannel(Channel* channel) const;


    // 创建并返回一个默认的 Poller 子类对象
    static Poller* newDefaultPoller(EventLoop* loop); // 在 DefaultPoller.cc中实现


protected:
    // map <key: sockfd,  value: sockfd所属的Channel通道类型>  内部管理所有已注册的通道对象
    using ChannelMap = std::unordered_map<int, Channel*>;
    ChannelMap channels_;


private:
    EventLoop *ownerLoop_; // 定义Poller所属的事件循环EventLoop

};