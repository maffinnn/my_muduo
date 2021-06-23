#pragma once

#include "Poller.h"

#include <vector>
#include <sys/epoll.h>

class Timestamp;

/*
    epoll的使用
    epoll_create
    epoll_ctl add/mod/del
    epoll_wait
*/
class EpollPoller : public Poller
{
public:
    EpollPoller(EventLoop* loop);
    ~EpollPoller() override;

    // 重写基类poller的方法
    Timestamp poll(int timeoutMs, ChannelList* activeChannels) override;
    void updateChannel(Channel* channel) override;
    void removeChannel(Channel* channel) override;

private:
    static const int kInitEventListSize = 16; //EventList默认size为16

    //填写活跃的连接
    void fillActiveChannels(int numEvents, ChannelList* activeChannels) const;
    // 更新channel通道
    void update(int operation, Channel* channel);

    int epollfd_;
    using EventList = std::vector<epoll_event>;
    EventList events_;
};