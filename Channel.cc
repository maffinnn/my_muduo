#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"

#include <cassert>
#include <sys/poll.h>

const int Channel::kNoneEvent = 0; //没有事件
//这里poll的几个宏与epoll的宏的值是完全一致且表达的意思也是一样的 所以即可用EPOLL 也可以用POLL
//muduo库是同时支持poll和epoll的 并将两者抽象成了poller这个类
//POLLIN -- there is data to read 
//POLLPRI -- there is urgent data to read
//POLLOUT -- writing will not be blocked
const int Channel::kReadEvent = POLLIN | POLLPRI;
const int Channel::kWriteEvent = POLLOUT;

//EventLoop包含一个ChannelList和一个poller
Channel::Channel(EventLoop* loop, int fd)
    : loop_(loop)
    , fd_(fd)
    , events_(0)
    , revents_(0)
    , status_(-1)
    , tied_(false)
    , eventHandling_(false)
    , addedToLoop_(false)
{
}

Channel::~Channel()
{
    assert(!eventHandling_);// 保证channel回调已返回 没有在操作回调
    assert(!addedToLoop_); // 保证channel要么从来没有被使用过 要么已经被removed掉了 才能正常的析构

    // 线程和事件循环是一一对应的关系
    // 这句话的意思是判断当前要析构的channel是否在它所该在的eventLoop里面 且在当前事件循环所在的线程里被析构
    // if(loop_->isInLoopThread())
    // {
    //     assert(!loop_->hasChannel(this));
    // }

}

//Channel的tie方法
// TcpConnection管理了一个channel
void Channel::tie(const std::shared_ptr<void>& obj)
{
    //std::weak_ptr<void> tie_ 是一个弱智能指针 作观察者 用于观察一个强智能指针obj
    tie_ = obj;
    tied_ = true;
}

// 作用是当改变channel所表示fd的events事件后 使用update在poller里面更新相应的事件epoll_ctl
void Channel::update()
{
    // 通过channel所属的eventloop调用poller的相应方法，注册fd的events事件
    addedToLoop_ = true;
    loop_->updateChannel(this); //this是当前channel
}

// 在channel所属的eventloop中调用remove将channel删除
void Channel::remove()
{
    assert(isNoneEvent());
    addedToLoop_ = false;
    loop_->removeChannel(this);
}

// fd得到poller通知以后 处理事件
void Channel::handleEvent(Timestamp receiveTime)
{
    if(tied_) // 判断是否绑定过
    {
        //成员变量 tie监听过一个当前的这个channel
         std::shared_ptr<void> guard = tie_.lock();// 将weak_ptr提升成强智能指针 
        //判断相应的当前的channel是否存活
        if(guard)
        {
            handleEventWithGuard(receiveTime);                                                                                                                                                                                                                                                                                                                                                                     
        }
    }
    else
    {
        //如果提升失败 即未被tie_过 或TcpConnection被remove掉了
        handleEventWithGuard(receiveTime);
    }
}
//POLLHUP -- Hang up output only(==only used to indicate the status of revents)
//POLLNVAL -- Invalid request:fd not open output only(==only used to indicate the status of revents)
// 根据poller通知的channel发生的具体事件，由channel负责调用相应的回调操作
void Channel::handleEventWithGuard(Timestamp receiveTime)
{
    eventHandling_ = true;
    LOG_INFO("channel handleEvent revents:%d\n",revents_);
    //如果有revents返回但是挂起状态 而且无法读取
    //发生关闭异常
    if ((revents_ & POLLHUP)&&!(revents_&POLLIN))
    {
        //如果closeCallback不为空
        if(closeCallback_) closeCallback_();

    }
    // 异常
    if(revents_ & (POLLERR|POLLNVAL))
    {
        if (errorCallback_) errorCallback_();
    }
    //有读事件
    if (revents_ & POLLIN | POLLPRI | POLLRDHUP)
    {
        if (readCallback_) readCallback_(receiveTime);
    }
    //有写事件
    if (revents_ & POLLOUT)
    {
        if (writeCallback_) writeCallback_();
    }

    eventHandling_ = false;
}