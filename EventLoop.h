#pragma once

#include "noncopyable.h"
#include "Timestamp.h"
#include "CurrentThread.h"

#include <functional>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>

class Channel;
class Poller;

// �¼�ѭ���� ��Ҫ����������ģ��
// ChannelList Poller(epoll�ĳ���)

class EventLoop
{
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    void loop(); // �����¼�ѭ��
    void quit(); // �˳��¼�ѭ��

    Timestamp pollReturnTime() const { return pollReturnTime_; }

    /*!!!*/
    void runInLoop(Functor cb); // �ڵ�ǰloop��ִ�лص�
    void queueInLoop(Functor cb); // ��c�ص���������� ����loop���ڵ��߳�ִ�лص�

    void wakeup(); // ����loop���ڵ��߳�
    //EventLoop�ķ�������Poller�ķ���
    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);
    bool hasChannel(Channel* channel);

    // thread_ �Ǵ���EventLoopʱ����¼��id CurrentThread::tid �ǵ�ǰ�̵߳�id
    // �����жϵ�ǰEventloop�Ƿ��ڴ��������߳��� �Ϳ�������ִ�лص� ��������ȴ�queueInLoop
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }

private:
    void handleRead(); // waked up
    void doPendingFunctors(); // ִ�лص�

    using ChannelList = std::vector<Channel*>;

    std::atomic_bool looping_; //ԭ�Ӳ����� ͨ��CASʵ�ֵ�
    std::atomic_bool quit_; // ��־�˳�loopѭ��
    bool eventHandling_; // ������ʹ��
 
    const pid_t threadId_; //��¼��ǰloop�����̵߳�id ��ʱ��ʹ��������߳�id�ȽϾ��ǵ����eventloop�Ƿ����Լ����߳�����
    Timestamp pollReturnTime_; // poller���ط����¼���channelsʱ���
    std::unique_ptr<Poller> poller_; // eventloop�������poller
    
    // !!!��mainReactor��ȡһ�����û���channel�� ͨ����ѯ�㷨subReactor���п�����˯������ͨ���ó�Ա����subReactor����channel
    int wakeupFd_; 
    std::unique_ptr<Channel> wakeupChannel_; // �ײ�ʹ�� ϵͳ����eventfd()

    ChannelList activeChannels_;
    Channel* currentActiveChannel_; // ������ʹ��

    std::atomic_bool callingPendingFunctors_; //��ʶ��ǰloop�Ƿ�����Ҫִ�еĻص�����
    std::vector<Functor> pendingFunctors_; // ���loop��Ҫִ�е����еĻص�����
    std::mutex mutex_; // ������������������pendingFunctors_�������̰߳�ȫ����
};  