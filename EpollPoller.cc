#include "EpollPoller.h"
#include "Logger.h"
#include "Channel.h"

#include <unistd.h>
#include <errno.h>
#include <cassert>
#include <string.h>

//channel�ĳ�Աstatus_��ʼ��Ϊ-1
const int kNew = -1; // ��ʾһ��channel��û����ӵ�epoll��
const int kAdded = 1; // �Ѿ����
const int kDeleted = 2; // �Ѿ�ɾ��

EpollPoller::EpollPoller(EventLoop* loop)
    :Poller(loop)//����Ҫ���û���Ĺ��캯������ʼ���ӻ���̳����ĳ�Ա
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

// channelͨ��loop_����updateChannel
// channel����������update/remove ��ײ�ͨ��Eventloop������poller��updateChannel
void EpollPoller::updateChannel(Channel* channel)
{
    const int status = channel->get_status();
    LOG_INFO("func=%s => fd=%d events=%d \n", __FUNCTION__, channel->get_fd(), channel->get_events());
    if (status == kNew || status == kDeleted)
    {
        //a new one, add with EPOLL_CTL_ADD
        int fd = channel->get_fd();
        if (status == kNew) // �µ�channel
        {
             assert(channels_.find(fd) == channels_.end());
             channels_[fd] = channel;
        }
        else // ��ɾ����channel
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
        //ȷ�ϵ�ǰchannelȷʵ������channelMap��
        assert(channels_.find(fd)!=channels_.end());
        assert(channels_[fd] == channel);
        assert(status == kAdded);
        if (channel->isNoneEvent())
        {
            //û���κθ���Ȥ���¼�
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
//��д��Ծ������
void EpollPoller::fillActiveChannels(int numEvents, 
                                ChannelList* activeChannels) const
{
    for (int i=0; i<numEvents; ++i)
    {
        Channel* channel = static_cast<Channel*>(events_[i].data.ptr); // ��ȡ����eventlist���channel
        channel->set_revents(events_[i].events);
        activeChannels->push_back(channel);// EventLoop���õ�������poller�������ص����з����¼���channel�б���

    }
}
// ����channelͨ�� epoll_ctl��add��modify�Ĳ���
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

//��EventLoop����poller.poll
//ͨ��epoll_wait �ѷ����¼���channelͨ������activeChannels��֪EventLoop
Timestamp EpollPoller::poll(int timeoutMs, ChannelList* activeChannels)
{
    //��Ϊpoll��������ڸ߲�������»ᱻ�����ĵ��� ��������LOG_INFO��Ӱ�쵽Ч��
    //ʹ��LOG_DEBUG�����Ϊ����
    LOG_INFO("func=%s => fd total count:%lu \n", __FUNCTION__, channels_.size());

    int nready = ::epoll_wait(epollfd_,          //vector�ײ��Ǹ�����
                                &(*events_.begin()),//begin()����������Ԫ�صĵ����� *�����þ��ǵ�����ָ��ĵ�һ��Ԫ�أ� ��ȡ��ַ
                                 static_cast<int>(events_.size()), //size()����unsigned_int ʹ��static_cast��ȫ��ǿת              //��ע���������Ҳ��һ����װ�õ���object �����ǽ�����ָ�룩
                                timeoutMs);
    
    int savedError = errno; //�ֲ���������errno errno��ȫ�ֵ� ��һ��eventloop����һ��poller ÿ��poller��epoll_wait��errno�᲻һ��
    Timestamp now(Timestamp::now());
    if (nready > 0)
    {
        LOG_INFO("numEvents=%d happened \n", nready);
        fillActiveChannels(nready, activeChannels);
        //���ݵĲ���
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
        // �д�����
        // EINTR interrupted system call �����ⲿ���ж�ʱ һ����˵����������Ҫ��������ҵ���߼�����
        if (savedError != EINTR) 
        {
            //�������Ĵ��������
            errno = savedError;
            LOG_ERROR("EpollPoller::poll() err! \n");
        }

    }
    return now;

}
