#include "EpollPoller.h"
#include "Logger.h"
#include "Channel.h"

#include <unistd.h>
#include <errno.h>
#include <cassert>
#include <string.h>

//channel的成员status_初始化为-1
const int kNew = -1; // 表示一个channel还没有添加到epoll里
const int kAdded = 1; // 已经添加
const int kDeleted = 2; // 已经删除

EpollPoller::EpollPoller(EventLoop* loop)
    :Poller(loop)//首先要调用基类的构造函数来初始化从基类继承来的成员
    , epollfd_(::epoll_create1(EPOLL_CLOEXEC))
    , events_(kInitEventListSize)
{
    if (epollfd_ < 0)
    {
        LOG_FATAL("epoll_create error:%d \n", errno);
    }
}

EpollPoller::~EpollPoller()
{
    ::close(epollfd_);
}
/*
               EventLoop
    ChannelList         Poller -> epollfd
                    ChannelMap < fd : channel* > channels_
*/

// channel通过loop_调用updateChannel
// channel调用自身函数update/remove 其底层通过Eventloop来调用poller的updateChannel
void EpollPoller::updateChannel(Channel* channel)
{
    const int status = channel->get_status();
    LOG_INFO("func=%s => fd=%d events=%d \n", __FUNCTION__, channel->get_fd(), channel->get_events());
    if (status == kNew || status == kDeleted)
    {
        //a new one, add with EPOLL_CTL_ADD
        int fd = channel->get_fd();
        if (status == kNew) // 新的channel
        {
             assert(channels_.find(fd) == channels_.end());
             channels_[fd] = channel;
        }
        else // 被删除的channel
       {
           assert(channels_.find(fd) != channels_.end());
           assert(channels_[fd] == channel);
       }
        channel->set_status(kAdded);
        update(EPOLL_CTL_ADD, channel);
    }
    else 
    {
        //update existing one with EPOLL_CTL_MOD/DEL
        int fd =  channel->get_fd();
        //确认当前channel确实存在于channelMap中
        assert(channels_.find(fd)!=channels_.end());
        assert(channels_[fd] == channel);
        assert(status == kAdded);
        if (channel->isNoneEvent())
        {
            //没有任何感性趣的事件
            update(EPOLL_CTL_DEL, channel);
            channel->set_status(kDeleted);
        }
        else 
        {
            update(EPOLL_CTL_MOD, channel);
        }
    }
}
void EpollPoller::removeChannel(Channel* channel)
{
    int fd = channel->get_fd();
    assert(channels_.find(fd)!=channels_.end());
    assert(channels_[fd] == channel);
    assert(channel->isNoneEvent());
    LOG_INFO("func=%s => fd=%d \n", __FUNCTION__, fd);

    int status = channel->get_status();
    assert(status == kAdded||status == kDeleted);

    size_t n = channels_.erase(fd);
    assert(n==1);

    if (status == kAdded)
    {
        update(EPOLL_CTL_DEL, channel);
    }

    channel->set_status(kNew);
}
//填写活跃的连接
void EpollPoller::fillActiveChannels(int numEvents, 
                                ChannelList* activeChannels) const
{
    for (int i=0; i<numEvents; ++i)
    {
        Channel* channel = static_cast<Channel*>(events_[i].data.ptr); // 获取存在eventlist里的channel
        channel->set_revents(events_[i].events);
        activeChannels->push_back(channel);// EventLoop就拿到了它的poller给它返回的所有发生事件的channel列表了

    }
}
// 更新channel通道 epoll_ctl中add或modify的操作
void EpollPoller::update(int operation, Channel* channel)
{
    struct epoll_event event;
    memset(&event, 0, sizeof event);
    int fd = channel->get_fd();
    event.events = channel->get_events();
    event.data.ptr = channel;
    

    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0)
    {
        if (operation == EPOLL_CTL_DEL)
        {
            LOG_ERROR("epoll_ctl del error: %d \n", errno);
        }
        else
        {
            LOG_FATAL("epoll_ctl del error: %d \n", errno);
        }
    }

}

//由EventLoop调用poller.poll
//通过epoll_wait 把发生事件的channel通过参数activeChannels告知EventLoop
Timestamp EpollPoller::poll(int timeoutMs, ChannelList* activeChannels)
{
    //因为poll这个函数在高并发情况下会被大量的调用 因此如果用LOG_INFO会影响到效率
    //使用LOG_DEBUG输出更为合理
    LOG_INFO("func=%s => fd total count:%lu \n", __FUNCTION__, channels_.size());

    int nready = ::epoll_wait(epollfd_,          //vector底层是个数组
                                &(*events_.begin()),//begin()返回容器首元素的迭代器 *解引用就是迭代器指向的第一个元素， 再取地址
                                 static_cast<int>(events_.size()), //size()返回unsigned_int 使用static_cast安全地强转              //（注意迭代器是也是一个封装好的类object 而并非仅仅是指针）
                                timeoutMs);
    
    int savedError = errno; //局部变量存贮errno errno是全局的 而一个eventloop就有一个poller 每个poller的epoll_wait的errno会不一样
    Timestamp now(Timestamp::now());
    if (nready > 0)
    {
        LOG_INFO("numEvents=%d happened \n", nready);
        fillActiveChannels(nready, activeChannels);
        //扩容的操作
        if (nready == events_.size())
        {
            events_.resize(events_.size()*2);
        }
    }
    else if (nready == 0)
    {
        LOG_DEBUG("%s timeout! \n", __FUNCTION__);
    }
    else
    {
        // 有错误发生
        // EINTR interrupted system call 当有外部的中断时 一般来说服务器还是要继续进行业务逻辑处理
        if (savedError != EINTR) 
        {
            //有其他的错误引起的
            errno = savedError;
            LOG_ERROR("EpollPoller::poll() err! \n");
        }

    }
    return now;

}
