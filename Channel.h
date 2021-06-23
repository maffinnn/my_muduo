#pragma once

#include "noncopyable.h"
#include "Timestamp.h"

#include <functional>
#include <memory>

//declaration
class EventLoop; 

/*
    理清EventLoop Channel， poller之间的关系 Reactor模型上对应的多路事件分发器demultiplexer
    Channel 理解为通道，封装了sockfd和其感兴趣的event，如EPOLLIN，EPOLLOUT事件
    还绑定了poller返回的具体事件

* This class doesn't own the file descriptor
  The file descriptor could be a socket, an eventfd, a timerfd or a signalfd
*/

class Channel : noncopyable
{
public:
    // 相当于typedef std::function<void()> EventCallback;
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(Timestamp)>;

    Channel(EventLoop* loop, int fd);
    ~Channel();

    // 处理事件 fd得到poller的通知以后 处理事件的
    void handleEvent(Timestamp receiveTime);

    // 设置回调函数对象
    // 使用std::move()的原因：cb和成员变量readCallback_都属于左值(有变量名字又有内存) 
    // 又因为std::function是个对象 对象一般假设占用内存是很大的
    // readCallback_ = cb 赋值操作消耗资源庞大 因为会拷贝构造std::function object
    // 这里使用readCallback_ = std::move(cb); 将左值cb转成右值 即将cb的资源直接转给readCallback_ 
    // 因为出了这个函数以后cb这个形参就不需要
    void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

    //!!!!
    // Tie this channel to the owner object managed by shared_ptr;
    // prevent the owner object being destroyed in handleEvent
    // 防止当channel被手动removed掉后，channel还在执行回调操作
    void tie(const std::shared_ptr<void>&);

    int get_fd() const { return fd_; }
    int get_events() const { return events_; }
    void set_revents(int revt) { revents_ = revt; } // 为poller提供接口设置返回事件
    bool isNoneEvent() const { return events_ == kNoneEvent; }

    //设置fd相应的事件状态
    void enableReading() { events_|= kReadEvent; update(); } // 将相应的位加上 置1
    void disableReading() { events_ &= ~kReadEvent; update(); } // 将相应的位去除 置0
    void enableWriting() { events_|=kWriteEvent; update(); }
    void disableWriting() { events_ &= ~kWriteEvent; update();}
    void disableAll() { events_ = kNoneEvent; update(); }
    bool isWriting() const { return events_ & kWriteEvent; } // 判断相应的位是否为1
    bool isReading() const { return events_ & kReadEvent; }

    int get_status() { return status_; }
    void set_status(int status) { status_ = status; }

    EventLoop* ownerLoop() { return loop_; } //eventloop 包含channel
    void remove();//删除channel用的

private:
    //  私有方法 只给内部调用
    void update();
    void handleEventWithGuard(Timestamp receiveTime);

    // 在channel内部定义了这三个mask其实就是将poll和epoll几个事件的宏统一起来了
    static const int kNoneEvent; // 没有事件
    static const int kReadEvent; // 读事件 相当于EPOLLIN
    static const int kWriteEvent; // 写事件 相当于EPOLLOUT

    EventLoop* loop_;// 表示当前channel属于哪一个事件循环
    const int fd_;   // fd, 即poller监听的对象 一个channel对应一个fd
    int events_;     // 注册fd感兴趣的事件
    int revents_;    // poller返回的具体发生的事件
    int status_;      // used by Poller

    // !!!对channel作跨线程的监听TcpConnection对象的生存状态 解决多线程访问共享对象的线程安全问题
    std::weak_ptr<void> tie_; // 防止当我们手动调用remove channel以后还在使用channel
    bool tied_;

    // 回调函数对象
    // 因为channel通道里面能够获知fd最终发生的具体事件revents，所以它负责调用具体事件的回调操作
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;

    //debug用
    bool eventHandling_; // 用于确保当channel析构的时候channel在执行的回调已经返回
    bool addedToLoop_;  // 用于确保在当前的channel析构的时候应将当前的channel remove掉了 一个channel要被remove掉后才能析构
    
};