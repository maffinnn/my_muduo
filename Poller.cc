#include "Poller.h"

Poller::Poller(EventLoop *loop)
    : ownerLoop_(loop)
{
}

//�жϴ����channel�Ƿ���ChannelMap channels_������
bool Poller::hasChannel(Channel* channel) const
{
    ChannelMap::const_iterator it = channels_.find(channel->get_fd());
    return (it!=channels_.end() && it->second == channel);
}