#pragma once

#include "Poller.h"

#include <vector>
#include <sys/epoll.h>

class Timestamp;

/*
    epoll��ʹ��
    epoll_create
    epoll_ctl add/mod/del
    epoll_wait
*/
class EpollPoller : public Poller
{
public:
    EpollPoller(EventLoop* loop);
    ~EpollPoller() override;

    // ��д����poller�ķ���
    Timestamp poll(int timeoutMs, ChannelList* activeChannels) override;
    void updateChannel(Channel* channel) override;
    void removeChannel(Channel* channel) override;

private:
    static const int kInitEventListSize = 16; //EventListĬ��sizeΪ16

    //��д��Ծ������
    void fillActiveChannels(int numEvents, ChannelList* activeChannels) const;
    // ����channelͨ��
    void update(int operation, Channel* channel);

    int epollfd_;
    using EventList = std::vector<epoll_event>;
    EventList events_;
};