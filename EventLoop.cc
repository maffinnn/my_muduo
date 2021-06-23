#include "EventLoop.h"
#include "Logger.h"
#include "Poller.h"
#include "Channel.h"

#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <cassert>
#include <memory>
#include <vector>

// ��ֹһ���̴߳������EventLoop
// ����һ��ȫ�ֵ�EventLoopָ����� ��һ��EventLoop��������ʱ ����ָ���Ǹ�����
// ��ô��һ���߳�������ȥ����һ��EventLoop�����ʱ�� ���ָ�벻Ϊ�� �Ͳ����ڴ�����
__thread EventLoop* t_loopInThisThread = nullptr;

// ����Ĭ�ϵ�poller IO���õĳ�ʱʱ��
const int kPollTimeMs = 10000;

// ����wakeupfd ��������subReactor��������������
int createEventfd()
{
    // eventf����֪ͨsleeping EventLoop������������
    int evtfd = ::eventfd(0, EFD_NONBLOCK |EFD_CLOEXEC);
    if (evtfd<0)
    {
        //֪ͨ��û��֪ͨ�� �ӵ��µ�channelҲû������ ���abort����
        LOG_FATAL("eventfd error: %d \n", errno);
    }
    return evtfd;
}

EventLoop::EventLoop()
    : looping_(false)
    , quit_(false)
    , eventHandling_(false)
    , callingPendingFunctors_(false)
    , threadId_(CurrentThread::tid())
    , poller_(Poller::newDefaultPoller(this))
    , wakeupFd_(createEventfd())
    , wakeupChannel_(new Channel(this, wakeupFd_))
    , currentActiveChannel_(nullptr)
{
    LOG_DEBUG("EventLoop created %p in thread %d \n", this, threadId_);
    if (t_loopInThisThread)
    {
        LOG_FATAL("Another EventLoop %p eixts in this thread %d \n", t_loopInThisThread, threadId_);
    }
    else 
    {
        t_loopInThisThread = this;
    }

    // ����wakeupFd���¼������Լ������¼���Ļص�����
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    // ÿһ��EventLoop��������wakeupChannel��EOPOLLIN�¼�
    wakeupChannel_->enableReading();

}

EventLoop::~EventLoop()
{
    // ��Դ����
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;

}

void EventLoop::handleRead()
{
    // ÿһ��subReactor�����˶��channel
    // ��mainReactor������һ����Ϣ��wakeupChannel
    // ���wakeupChannel�ܹ���֪�����wakefd���пɶ��¼� �ͻ����˲�����ȥ�����¼���
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof one);
    if (n!=sizeof one)
    {
        LOG_ERROR("EventLoop::handleRead() reads %ld bytes instead of 8 bytes", n);
    }
}

// �����ײ�poller��ִ��poll
void EventLoop::loop()
{
    assert(!looping_);
    looping_ = true;
    quit_ = false;
    LOG_INFO("EventLoop %p start looping \n", this);

    while(!quit_)
    {
        activeChannels_.clear();
        // ��������fd �� 1. client��fd  2. wakeupFd
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
        eventHandling_ = true;

        for (Channel* channel: activeChannels_)
        {
            // Poller������Щchannel�����¼��ˣ� Ȼ���ϱ���EvetLoop��֪ͨchannel������Ӧ���¼�
            currentActiveChannel_ = channel; 
            currentActiveChannel_->handleEvent(pollReturnTime_);
        }

        currentActiveChannel_ = nullptr;
        eventHandling_ = false;
        // ִ�е�ǰEventLoop�¼�ѭ����Ҫ����Ļص�����
        /*
            IO�߳� mainLoop accept ���û����� ����fd ��channel�����fd 
            Ȼ���������û��ַ���subLoop ����subloop
            mainLoop����ע��һ���ص� ��ҪsubLoop��ִ��
            Ȼ����ʱsubLoop����˯��
            ֻ�е�mainLoop wakeup()ͨ��eventfd�Ժ� ִ��mainLoopע��Ļص�����
        */
        doPendingFunctors();

    }

    LOG_INFO("EventLoop %p stop looping \n", this);
    looping_ = false;

}

// �˳��¼�ѭ��
// ���������ֱ��������������1. loop���Լ����߳��е���quit
//                         2. �������̵߳������Լ�loop��quit
void EventLoop::quit()
{
    quit_ = true;
    // !!!������������߳��е�����quit e.g.��һ��subLoop��worker�̣߳��е�����mainLoop��IO����quit
    // muduo�Ⲣû��ʹ��һ����������IO�߳���worker�߳�֮����һ������ ��IO�߳��õ��µ�channel�Ժ�channel��������� Ȼ����worker�߳���ȡ��Щchannel
    // There is a chance that loop() just executes while(!quit_) and exits,
    // then EventLoop destructs, then we are accessing an invalid object.
    // Can be fixed using mutex_ in both places
    if (!isInLoopThread()) 
    {
        wakeup();
    }

}

// �ڵ�ǰloop��ִ�лص�
void EventLoop::runInLoop(Functor cb)
{
    if (isInLoopThread())// �ж��Ƿ��ڵ�ǰ�߳�
    {
        cb();
    }
    else
    {
        // �ڷǵ�ǰ�߳���ִ��cb�� ����Ҫ����loop���ڵ��߳�ִ��cb
        queueInLoop(std::move(cb));
    }

}
// ��cb�ص���������� ����loop���ڵ��߳�ִ�лص�
void EventLoop::queueInLoop(Functor cb)
{
    // ��cb����pendingFunctors_������ �漰����������
    // �����ж��subLoopͬʱ����runInLoopִ�лص� ����������п��ܻᱻ����̷߳��� ���ʹ����
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb); // push_back()�ǿ������� emplace_back()��ֱ�ӹ���
    }

    // ������Ӧ��,��Ҫִ������ص�������loop���߳�
    // || callingPendingFunctors_����˼�ǣ���ǰloop����ִ�лص� ����loop�������µĻص�
    if (!isInLoopThread() || callingPendingFunctors_)
    {
        wakeup(); // ����loop���ڵ��߳�
    }
}

void EventLoop::doPendingFunctors()
{

    // ����ֲ�functors�൱�ڽ�pendingFunctors_ �ÿ� 
    // ����pendingFunctors���Functorsת�Ƶ��ֲ������functors
    // ����ִ�еĻص�ȫ���ŵ��ֲ�����functors����
    // �ô�: doPendingFunctors()��������Ĺ�������pendingFunctors_�ﲻ��ѭ������һ���ص���ִ��
    //       ���Ծ����ó���һ��functor�ʹ�pendingFunctors_����ɾ��һ��
    //       ������loop����doPendingFunctor��ʱ�� mainLoopҲ������pendingFunctors_ע������ע��ص�
    //       Ȼ��pendingFunctors_ ����ס�� �������Ļص��ܶ� ��û����ô�챻�ͷŵ� 
    //       ��ô��ʱ��mainLoop�ᱻ�������� Ӱ���������ʱ��
    //       ����ľֲ�functor���൱��һ��������
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);// ��������൱�ڽ�pendingFunctors_�ͷŵ�
        // ���˴��������ͱ��ͷŵ���
    }

    for (const Functor& functor : functors)
    {
        functor(); //ִ�е�ǰloop����Ҫִ�еĻص�����
    }

    callingPendingFunctors_ = false;

}

// ����loop���ڵ��߳� 
// ��wakeupFdдһ������ �������wakeupChannel�������ݿɶ���(�������¼�) ������Ӧ��
// �ͻ���poller��poll�������������� poll����
void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof one);
    // д����д�������ζ�����̸߳����޷�������
    if (n!=sizeof one)
    {
        LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8 bytes \n", n);
    }

}
//EventLoop�ķ�������Poller�ķ���
void EventLoop::updateChannel(Channel* channel)
{
    assert(channel->ownerLoop() == this);
    poller_->updateChannel(channel);
}
void EventLoop::removeChannel(Channel* channel)
{
    assert(channel->ownerLoop()==this);
    poller_->removeChannel(channel);
}
bool EventLoop::hasChannel(Channel* channel)
{
    return poller_->hasChannel(channel);
}