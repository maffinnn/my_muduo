#include "EventLoopThread.h"
#include "EventLoop.h"
#include "Thread.h"

EventLoopThread::EventLoopThread(const ThreadInitCallback& cb, const std::string& name)
        : loop_(nullptr)
        , exiting_(false)
        , thread_(std::bind(&EventLoopThread::threadFunc, this), name) // 还没有真正创建线程 只是初始化了一个线程类的实例
        , mutex_()
        , cond_()
        , callback_(cb)
{
}


EventLoopThread::~EventLoopThread()
{
    exiting_ = true;
    if (loop_!=nullptr)
    {
        loop_->quit(); // 将线程里绑定的事件循环退出
        thread_.join();
    }
}

// 这个函数运行在父线程中
EventLoop* EventLoopThread::startLoop()
{
    // 启动底层新线程 执行的线程函数就是当前EventLoop在构造是所传入的threadFunc
    thread_.start();

    EventLoop* loop = nullptr;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        // 当loop还没起来时 那就一直等待在互斥锁上 直到收到下面的通知
        while(loop_ == nullptr)
        {
            cond_.wait(lock);
        }
        loop = loop_;
    }
    return loop;// 返回绑定的loop对象的地址
}

// 下面这个方法是在单独的新线程里面运行的
void EventLoopThread::threadFunc()
{
    // 创建一个独立的EventLoop 和上面的线程是一一对应的
    EventLoop loop;

    if (callback_) // 如果用户有绑定ThreadInit回调
    {
        callback_(&loop); // 就将loop传给ThreadInitCallback 做一些用户自定义的操作
    }

    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one();// 得等这个loop起来了 再通知当前线程 才可以访问
    }

    loop.loop(); // 开启底层EventLoop的事件循环监听函数 poller.poll

    // 当loop返回了 服务器的程序要关闭了 不在进行事件循环了
    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = nullptr;

}