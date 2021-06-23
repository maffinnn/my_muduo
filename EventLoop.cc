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

// 防止一个线程创建多个EventLoop
// 这是一个全局的EventLoop指针变量 当一个EventLoop创建起来时 它就指向那个对象
// 那么在一个线程里面再去创建一个EventLoop对象的时候 这个指针不为空 就不会在创建了
__thread EventLoop* t_loopInThisThread = nullptr;

// 定义默认的poller IO复用的超时时间
const int kPollTimeMs = 10000;

// 创建wakeupfd 用来唤醒subReactor处理新来的链接
int createEventfd()
{
    // eventf用来通知sleeping EventLoop来处理新链接
    int evtfd = ::eventfd(0, EFD_NONBLOCK |EFD_CLOEXEC);
    if (evtfd<0)
    {
        //通知都没法通知了 接到新的channel也没法处理 因此abort程序
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

    // 设置wakeupFd的事件类型以及发生事件后的回调操作
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    // 每一个EventLoop都将监听wakeupChannel的EOPOLLIN事件
    wakeupChannel_->enableReading();

}

EventLoop::~EventLoop()
{
    // 资源回收
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;

}

void EventLoop::handleRead()
{
    // 每一个subReactor监听了多个channel
    // 当mainReactor发送了一个消息给wakeupChannel
    // 这个wakeupChannel能够感知到这个wakefd上有可读事件 就唤起了并可以去处理事件了
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof one);
    if (n!=sizeof one)
    {
        LOG_ERROR("EventLoop::handleRead() reads %ld bytes instead of 8 bytes", n);
    }
}

// 驱动底层poller来执行poll
void EventLoop::loop()
{
    assert(!looping_);
    looping_ = true;
    quit_ = false;
    LOG_INFO("EventLoop %p start looping \n", this);

    while(!quit_)
    {
        activeChannels_.clear();
        // 监听两类fd ： 1. client的fd  2. wakeupFd
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
        eventHandling_ = true;

        for (Channel* channel: activeChannels_)
        {
            // Poller监听哪些channel发生事件了， 然后上报给EvetLoop，通知channel处理相应的事件
            currentActiveChannel_ = channel; 
            currentActiveChannel_->handleEvent(pollReturnTime_);
        }

        currentActiveChannel_ = nullptr;
        eventHandling_ = false;
        // 执行当前EventLoop事件循环需要处理的回调操作
        /*
            IO线程 mainLoop accept 新用户链接 返回fd 用channel来打包fd 
            然后将已连接用户分发给subLoop 唤起subloop
            mainLoop事先注册一个回调 需要subLoop来执行
            然而此时subLoop还在睡觉
            只有当mainLoop wakeup()通过eventfd以后 执行mainLoop注册的回调操作
        */
        doPendingFunctors();

    }

    LOG_INFO("EventLoop %p stop looping \n", this);
    looping_ = false;

}

// 退出事件循环
// 可能有两种被调用情况发生：1. loop在自己的线程中调用quit
//                         2. 被其他线程调用了自己loop的quit
void EventLoop::quit()
{
    quit_ = true;
    // !!!如果是在其他线程中调用了quit e.g.在一个subLoop（worker线程）中调用了mainLoop（IO）的quit
    // muduo库并没有使用一个队列在主IO线程与worker线程之间做一个缓冲 当IO线程拿到新的channel以后将channel放入队列中 然后由worker线程来取这些channel
    // There is a chance that loop() just executes while(!quit_) and exits,
    // then EventLoop destructs, then we are accessing an invalid object.
    // Can be fixed using mutex_ in both places
    if (!isInLoopThread()) 
    {
        wakeup();
    }

}

// 在当前loop中执行回调
void EventLoop::runInLoop(Functor cb)
{
    if (isInLoopThread())// 判断是否在当前线程
    {
        cb();
    }
    else
    {
        // 在非当前线程中执行cb， 就需要唤醒loop所在的线程执行cb
        queueInLoop(std::move(cb));
    }

}
// 把cb回调放入队列中 唤醒loop所在的线程执行回调
void EventLoop::queueInLoop(Functor cb)
{
    // 将cb放入pendingFunctors_队列中 涉及到并发操作
    // 可能有多个subLoop同时调用runInLoop执行回调 这样这个队列可能会被多个线程访问 因此使用锁
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb); // push_back()是拷贝构造 emplace_back()是直接构造
    }

    // 唤醒相应的,需要执行上面回调操作的loop的线程
    // || callingPendingFunctors_的意思是：当前loop正在执行回调 但是loop又有了新的回调
    if (!isInLoopThread() || callingPendingFunctors_)
    {
        wakeup(); // 唤醒loop所在的线程
    }
}

void EventLoop::doPendingFunctors()
{

    // 定义局部functors相当于将pendingFunctors_ 置空 
    // 即将pendingFunctors里的Functors转移到局部定义的functors
    // 将待执行的回调全部放到局部变量functors里面
    // 好处: doPendingFunctors()这个函数的工作是在pendingFunctors_里不断循环地拿一个回调并执行
    //       所以就是拿出来一个functor就从pendingFunctors_里面删除一个
    //       肯能子loop在做doPendingFunctor的时候 mainLoop也正在往pendingFunctors_注册里面注册回调
    //       然而pendingFunctors_ 被锁住了 如果里面的回调很多 锁没有这么快被释放掉 
    //       那么这时候mainLoop会被阻塞在外 影响服务器的时延
    //       这里的局部functor就相当于一个缓冲区
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);// 交换完就相当于将pendingFunctors_释放掉
        // 出了大括号锁就被释放掉了
    }

    for (const Functor& functor : functors)
    {
        functor(); //执行当前loop里需要执行的回调操作
    }

    callingPendingFunctors_ = false;

}

// 唤醒loop所在的线程 
// 向wakeupFd写一个数据 这样这个wakeupChannel就有数据可读了(发生读事件) 便有响应了
// 就会在poller的poll处（阻塞）唤醒 poll返回
void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof one);
    // 写数据写错可能意味着子线程根本无法被唤醒
    if (n!=sizeof one)
    {
        LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8 bytes \n", n);
    }

}
//EventLoop的方法调用Poller的方法
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