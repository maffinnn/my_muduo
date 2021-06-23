#include "Poller.h"

Poller::Poller(EventLoop *loop)
    : ownerLoop_(loop)
{
}

//判断传入的channel是否在ChannelMap channels_保存着
bool Poller::hasChannel(Channel* channel) const
{
    ChannelMap::const_iterator it = channels_.find(channel->get_fd());
    return (it!=channels_.end() && it->second == channel);
}