#include <sys/epoll.h> // 提供 epoll 事件类型 EPOLLIN、EPOLLOUT 等

#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"


// 事件定义
const int Channel::kNoneEvent  = 0;                  // 不监听任何事件       0 
const int Channel::kReadEvent  = EPOLLIN | EPOLLPRI; // 监听EPOLLIN（可读）  0x001 | 0x002 = 0x003 = 3
const int Channel::kWriteEvent = EPOLLOUT;           // 监听EPOLLOUT（可写） 0x004 = 4


// 构造和析构
Channel::Channel(EventLoop *loop, int fd) 
    : loop_(loop)
    , fd_(fd)
    , events_(0)
    , revents_(0)
    , index_(-1)   // EPollPoller中的 kNew = -1 "channel未添加到Poller"
    , tied_(false)
{

}

Channel::~Channel() 
{

}

// channel的tie方法什么时候调用过?  TcpConnection => channel
/**
 * TcpConnection中注册了Channel对应的回调函数，传入的回调函数均为TcpConnection对象的成员方法，
 * 因此可以说明一点就是：Channel的结束一定晚于TcpConnection对象！
 * 此处用tie去解决TcpConnection和Channel的生命周期时长问题，从而保证了Channel对象能够在TcpConnection销毁前销毁。
 **/

 
 // tie() 绑定TcpConnection
void Channel::tie(const std::shared_ptr<void> &obj) // tie_观察传进来的智能指针 obj
{
    tie_ = obj;   // tie_ 绑定 TcpConnection，保证 TcpConnection 不被提前销毁
    tied_ = true; // 标记已绑定
}



//update 和 remove => EpollPoller 更新channel在poller中的状态
/**
 * 当改变channel所表示的fd的events事件后，update负责在poller里面更改fd相应的事件epoll_ctl
 **/

// 当Channel关注的事件改变时，调用update()通知EventLoop，再由EventLoop调用Poller的epoll_ctl()更新监听事件
void Channel::update()
{
   // 通过Channel所属的EventLoop，调用poller的相应方法，注册fd的events
   loop_->updateChannel(this); 
}


// 在Channel所属的EventLoop中把当前的Channel删除
 void Channel::remove()
 {
    loop_->removeChannel(this);
 }



 // handleEvent() 事件处理
 void Channel::handleEvent(Timestamp receiveTime)
 {
    if (tied_) // 如果Channel绑定了TcpConnection
    {
        std::shared_ptr<void> guard = tie_.lock(); // 先尝试lock()提升weak_ptr，确保TcpConnection还在
        if (guard) // 如果TcpConnection还在，就调用handleEventWithGuard()处理事件
        {
            handleEventWithGuard(receiveTime);
        }
        // 如果lock提升失败了，就不做任何处理了，说明Channel的TcpConnection对象已经不存在了
    }
    else 
    {
        handleEventWithGuard(receiveTime);
    }
 }


 // handleEventWithGuard  根据Poller通知Channel发生的具体事件，由Channel负责调用具体的回调
 void Channel::handleEventWithGuard(Timestamp receiveTime)
 {
    // 日志记录revents_
    LOG_INFO("channel handleEvent revents:%d\n", revents_);

    // 关闭or挂起事件 EPOLLHUB
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) 
    {                                                   
        // 当EPOLLHUB发生，意味着对端已经关闭连接
        // 如果没有EPOLLIN事件（表示已经没有数据可读），触发closeCallback_()关闭回调
        if (closeCallback_)
        {
            closeCallback_();
        }
    }
    // 错误事件 EPOLLERR
    if (revents_ & EPOLLERR)
    {
        if (errorCallback_)
        {
            errorCallback_();
        }
    }
    // 读事件 EPOLLIN | EPOLLPRI
    if (revents_ & (EPOLLIN | EPOLLPRI))
    {
        if (readCallback_)
        {
            readCallback_(receiveTime);// 调用readCallback_(), 传递receiveTime（事件发生时间）
        }
    }
    // 写事件 EPOLLOUT
    if (revents_ & EPOLLOUT)
    {
        if (writeCallback_)
        {
            writeCallback_();
        }
    }
 }