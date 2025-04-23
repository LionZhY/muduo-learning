#include <errno.h>  // 提供错误码 errno
#include <unistd.h> // 提供系统调用 如close()
#include <string.h> 

#include "EPollPoller.h"
#include "Logger.h"
#include "Channel.h"


// index (标识 channel 当前状态)
const int kNew = -1;    // 某个channel还未添加至poller      // channel的成员index_初始化为-1
const int kAdded = 1;   // 某个channel已经添加至poller
const int kDeleted = 2; // 某个channel已经从poller移除(曾添加过，现已删除)


// 构造 
EPollPoller::EPollPoller(EventLoop* loop)
    : Poller(loop)                              // 初始化基类Poller，传入所属的EventLoop指针
    , epollfd_(::epoll_create1(EPOLL_CLOEXEC))  // epoll_create1() 创建 epoll 实例，返回epoll的fd
    , events_(kInitEventListSize)               // 初始化events_容器大小 vector<epoll_events>(16)
{
    if (epollfd_ < 0) // // 检查 epoll_create1 否返回负值（创建失败，返回-1）
    {
        LOG_FATAL("epoll_create error:%d \n", errno); // 打印致命错误日志(并Logger内部调用exit终止程序)
    }
}



// 虚析构 （重写父类的虚析构）
EPollPoller::~EPollPoller()  
{
    ::close(epollfd_); // 关闭epoll文件描述符资源
}


/*
**重写基类Poller的（纯虚函数）方法  poll()  updateChannel()  removeChannel()
*/

// 封装 epoll_wait   填充events_ activeChannels 记录发生事件数量numEvents
Timestamp EPollPoller::poll(int timeoutMs, ChannelList* activeChannels)
{
    /**
     * timeoutMs：     超时时间（毫秒），传给 epoll_wait
     * activeChannels：用于输出发生事件的 Channel 指针列表（fiiActiveChannels填充）
     */

    // 打印日志：当前注册在epoll中的channel总数量 （实际部署建议使用 LOG_DEBUG 或关闭）
    LOG_INFO("func=%s => fd total count:%lu\n", __FUNCTION__, channels_.size());
    

    // 调用 epoll_wait 
    int numEvents = ::epoll_wait( // 返回值numEvents 表示发生事件的数量 (0 表示超时，-1 表示错误)
        epollfd_,                         // epoll实例fd
        &*events_.begin(),                // 传入vector<epoll_event> events_ 首地址，作为输出数组
        static_cast<int>(events_.size()), // 当前监听的最大fd数量（events_的容量）
        timeoutMs                         // 最大等待时间（毫秒），-1 表示永久阻塞。
    );

    // 保存errno，防止被后续函数覆盖
    int saveErrno = errno; 
    
    // 获取当前时间戳 标记本次 poll 的返回时间
    Timestamp now(Timestamp::now()); 

    
    // 处理事件
    if (numEvents > 0) // 有事件发生
    {
        LOG_INFO("%d events happend\n", numEvents);     // 打印日志
        fillActiveChannels(numEvents, activeChannels);  // 填充活跃的channel到EventLoop
        if (numEvents == events_.size()) // 若事件数组已满，扩容到2倍
        {
            events_.resize(events_.size() * 2);
        }
    }
    else if (numEvents == 0) // 超时返回，无事件发生
    {
        LOG_DEBUG("%s timeout!\n", __FUNCTION__);
    }
    else // numEvents < 0 出错
    {
        if (saveErrno != EINTR) // 检查错误码 若不是被信号中断
        {
            errno = saveErrno;
            LOG_ERROR("EPollPoller::poll() errot!"); // 打印错误日志
        }
    }
    
    return now; // 返回poll返回时的时间戳
}



/**
 * Channel::update => EventLoop::updateChannel => Poller::updateChannel
 * Channel::remove => EventLoop::removeChannel => Poller::removeChannel
 */

// 添加或更新一个Channel的事件监听状态（在channel中调用）
void EPollPoller::updateChannel(Channel* channel)
{
    const int index = channel->index(); // 获取当前channel的状态 kNew(-1) kAdded(1) kDeleted(2)

    // 打印日志 “所属函数名  fd  监听的事件类型  当前index状态”
    LOG_INFO("func=%s => fd=%d events=%d index=%d \n", __FUNCTION__, channel->fd(), channel->events(), index);

    // kNew未添加，kDeleted已删除状态  (新添加的Channel，或者是曾经被删除的Channel，需要重新ADD到epoll中)
    if (index == kNew || index == kDeleted) 
    {
        // 未添加
        if (index == kNew) 
        {
            int fd = channel->fd();  // 获取fd
            channels_[fd] = channel; // 添加到channelmap中 <fd, fd所属的channel类型>
        }
        // 已删除 
        else 
        {
            // index == kDeleted 的话，说明 map 里原本就有它，因此不需要再次插入，只要设置状态+update
        }

        channel->set_index(kAdded);     // 设置为已添加状态
        update(EPOLL_CTL_ADD, channel); // 调用epoll_ctl添加到epoll中
    }
    // kAdded 已添加
    else 
    {
        int fd = channel->fd();
        // 根据是否监听事件判断是否要删除或修改
        if (channel->isNoneEvent()) // channel不再关注任何事件，不需要Poller监听任何事件了
        {
            update(EPOLL_CTL_DEL, channel); // 从epoll中移除channel对应的fd
            channel->set_index(kDeleted);   // 状态设置为已删除
        }
        else 
        {
            update(EPOLL_CTL_MOD, channel); // 修改监听事件
        }
    }

}


// 从poller中移除不再需要监听的channel （在Channel::remove()中调用）
void EPollPoller::removeChannel(Channel* channel)
{
    int fd = channel->fd(); // 获取channel对应的fd
    channels_.erase(fd);    // 从 fd->Channel 映射表 <fd, Channel*> 中删除

    LOG_INFO("func=%s => fd=%d\n", __FUNCTION__, fd);

    int index = channel->index(); // 获取当前 channel 的状态
    if (index == kAdded) // 若在 epoll 中注册过
    {
        update(EPOLL_CTL_DEL, channel); // 从epoll中注销
    }

    channel->set_index(kNew); // 设置为“未添加”状态
}




// 将就绪事件对应的 Channel 填入 activeChannels
void EPollPoller::fillActiveChannels(int numEvents, ChannelList* activeChannels) const
{
    // for 循环处理所有就绪事件
    for (int i = 0; i < numEvents; ++i) // numEvents表示epoll_wait()返回的就绪事件数
    {
        Channel* channel = static_cast<Channel*>(events_[i].data.ptr); // 获取epoll_event对应的channel指针
        channel->set_revents(events_[i].events);  // 设置实际触发的事件
        activeChannels->push_back(channel);       // 就绪的 Channel* 加入 activeChannels 列表中
    }
}



// 封装epoll_ctl , 更新channel通道 (其实就是调用epoll_ctl add/mod/del)
void EPollPoller::update(int operation, Channel* channel)
{
    epoll_event event; // 创建一个 epoll_event 结构体对象，用于传递给 epoll_ctl()
    ::memset(&event, 0, sizeof(event)); // 清空结构体，防止脏数据

    int fd = channel->fd();

    event.events   = channel->events(); // 设置关心的事件
    event.data.fd  = fd;
    event.data.ptr = channel;           // 传入channel指针，用于回调

    // 核心调用：执行 epoll_ctl()
    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0) // 如果执行失败
    {
        if (operation == EPOLL_CTL_DEL) // 如果del删除失败（非致命错误）
        {
            LOG_ERROR("epoll_ctl del error:%d\n", errno);       // 输出错误日志，不中断程序
        }
        else // 如果是add或mod失败（说明程序逻辑严重出错）
        {
            LOG_FATAL("epoll_ctl add/mod error:%d\n", errno);   // 输出错误日志，并终止程序
        }
    }
}