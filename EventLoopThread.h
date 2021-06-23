#pragma once

#include "noncopyable.h"
#include "Thread.h"

#include <functional>
#include <mutex>
#include <condition_variable>
#include <string>

class EventLoop;

class EventLoopThread : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    EventLoopThread(const ThreadInitCallback& cb = ThreadInitCallback(), 
                        const std::string& name = std::string());
    ~EventLoopThread();
    EventLoop* startLoop();

private:
    void threadFunc();

    EventLoop* loop_; // 这个loop指针指向的对象就是运行在下面thread_线程里的对象
    bool exiting_;
    Thread thread_;
    std::mutex mutex_; // 互斥锁
    std::condition_variable cond_; // 条件变量
    ThreadInitCallback callback_;

};