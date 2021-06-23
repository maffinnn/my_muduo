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

// 事件循环类 主要包含了两大模块
// ChannelList Poller(epoll的抽象)

class EventLoop
{
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    void loop(); // 开启事件循环
    void quit(); // 退出事件循环

    Timestamp pollReturnTime() const { return pollReturnTime_; }

    /*!!!*/
    void runInLoop(Functor cb); // 在当前loop中执行回调
    void queueInLoop(Functor cb); // 把c回调放入队列中 唤醒loop所在的线程执行回调

    void wakeup(); // 唤醒loop所在的线程
    //EventLoop的方法调用Poller的方法
    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);
    bool hasChannel(Channel* channel);

    // thread_ 是创建EventLoop时所记录的id CurrentThread::tid 是当前线程的id
    // 用于判断当前Eventloop是否在创建它的线程中 就可以正常执行回调 否则继续等待queueInLoop
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }

private:
    void handleRead(); // waked up
    void doPendingFunctors(); // 执行回调

    using ChannelList = std::vector<Channel*>;

    std::atomic_bool looping_; //原子操作， 通过CAS实现的
    std::atomic_bool quit_; // 标志退出loop循环
    bool eventHandling_; // 做断言使用
 
    const pid_t threadId_; //记录当前loop所在线程的id 到时候使用这个与线程id比较就是到这个eventloop是否在自己的线程里面
    Timestamp pollReturnTime_; // poller返回发生事件的channels时间点
    std::unique_ptr<Poller> poller_; // eventloop所管理的poller
    
    // !!!当mainReactor获取一个新用户的channel， 通过轮询算法subReactor（有可能再睡觉），通过该成员唤醒subReactor处理channel
    int wakeupFd_; 
    std::unique_ptr<Channel> wakeupChannel_; // 底层使用 系统调用eventfd()

    ChannelList activeChannels_;
    Channel* currentActiveChannel_; // 做断言使用

    std::atomic_bool callingPendingFunctors_; //标识当前loop是否有需要执行的回调操作
    std::vector<Functor> pendingFunctors_; // 存放loop需要执行的所有的回调操作
    std::mutex mutex_; // 互斥锁用来保护上面pendingFunctors_容器的线程安全操作
};  