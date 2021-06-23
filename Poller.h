#pragma once

#include "noncopyable.h"
#include "Timestamp.h"
#include <vector>
#include <unordered_map>

#include "Channel.h"

class EventLoop;

//muduo���ж�·ʱ��ַ����ĺ���IO����ģ��
//Poller ��������EventLoop���汣���channels
class Poller : noncopyable
{
public:
    using ChannelList = std::vector<Channel*>;

    Poller(EventLoop *loop);
    virtual ~Poller() = default;

    // ������IO���ñ���ͳһ�Ľӿ�
    virtual Timestamp poll(int timeoutMs, ChannelList* activeChannels) = 0;
    virtual void updateChannel(Channel* channel) = 0;
    virtual void removeChannel(Channel* channel) = 0;

    // �жϲ���channel�Ƿ��ڵ�ǰpoller����
    bool hasChannel(Channel* channel) const;

    // EventLoop����ͨ���ýӿڻ�ȡĬ�ϵ�IO���õľ���ʵ��
    static Poller* newDefaultPoller(EventLoop* loop);

protected:
    // û�б�Ҫʹ��map��Ĭ������unordered_map ��ʵ�ǹ�ϣ��Ч�ʸߣ�
    // ChannelMap��key����sockfd��Ӧ��value����sockfd������channelͨ������
    using ChannelMap = std::unordered_map<int, Channel*>;
    ChannelMap channels_;

private:
    EventLoop *ownerLoop_; // ����Poller�������¼�ѭ��
};