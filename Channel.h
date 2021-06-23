#pragma once

#include "noncopyable.h"
#include "Timestamp.h"

#include <functional>
#include <memory>

//declaration
class EventLoop; 

/*
    ����EventLoop Channel�� poller֮��Ĺ�ϵ Reactorģ���϶�Ӧ�Ķ�·�¼��ַ���demultiplexer
    Channel ���Ϊͨ������װ��sockfd�������Ȥ��event����EPOLLIN��EPOLLOUT�¼�
    ������poller���صľ����¼�

* This class doesn't own the file descriptor
  The file descriptor could be a socket, an eventfd, a timerfd or a signalfd
*/

class Channel : noncopyable
{
public:
    // �൱��typedef std::function<void()> EventCallback;
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(Timestamp)>;

    Channel(EventLoop* loop, int fd);
    ~Channel();

    // �����¼� fd�õ�poller��֪ͨ�Ժ� �����¼���
    void handleEvent(Timestamp receiveTime);

    // ���ûص���������
    // ʹ��std::move()��ԭ��cb�ͳ�Ա����readCallback_��������ֵ(�б������������ڴ�) 
    // ����Ϊstd::function�Ǹ����� ����һ�����ռ���ڴ��Ǻܴ��
    // readCallback_ = cb ��ֵ����������Դ�Ӵ� ��Ϊ�´������std::function object
    // ����ʹ��readCallback_ = std::move(cb); ����ֵcbת����ֵ ����cb����Դֱ��ת��readCallback_ 
    // ��Ϊ������������Ժ�cb����βξͲ���Ҫ
    void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

    //!!!!
    // Tie this channel to the owner object managed by shared_ptr;
    // prevent the owner object being destroyed in handleEvent
    // ��ֹ��channel���ֶ�removed����channel����ִ�лص�����
    void tie(const std::shared_ptr<void>&);

    int get_fd() const { return fd_; }
    int get_events() const { return events_; }
    void set_revents(int revt) { revents_ = revt; } // Ϊpoller�ṩ�ӿ����÷����¼�
    bool isNoneEvent() const { return events_ == kNoneEvent; }

    //����fd��Ӧ���¼�״̬
    void enableReading() { events_|= kReadEvent; update(); } // ����Ӧ��λ���� ��1
    void disableReading() { events_ &= ~kReadEvent; update(); } // ����Ӧ��λȥ�� ��0
    void enableWriting() { events_|=kWriteEvent; update(); }
    void disableWriting() { events_ &= ~kWriteEvent; update();}
    void disableAll() { events_ = kNoneEvent; update(); }
    bool isWriting() const { return events_ & kWriteEvent; } // �ж���Ӧ��λ�Ƿ�Ϊ1
    bool isReading() const { return events_ & kReadEvent; }

    int get_status() { return status_; }
    void set_status(int status) { status_ = status; }

    EventLoop* ownerLoop() { return loop_; } //eventloop ����channel
    void remove();//ɾ��channel�õ�

private:
    //  ˽�з��� ֻ���ڲ�����
    void update();
    void handleEventWithGuard(Timestamp receiveTime);

    // ��channel�ڲ�������������mask��ʵ���ǽ�poll��epoll�����¼��ĺ�ͳһ������
    static const int kNoneEvent; // û���¼�
    static const int kReadEvent; // ���¼� �൱��EPOLLIN
    static const int kWriteEvent; // д�¼� �൱��EPOLLOUT

    EventLoop* loop_;// ��ʾ��ǰchannel������һ���¼�ѭ��
    const int fd_;   // fd, ��poller�����Ķ��� һ��channel��Ӧһ��fd
    int events_;     // ע��fd����Ȥ���¼�
    int revents_;    // poller���صľ��巢�����¼�
    int status_;      // used by Poller

    // !!!��channel�����̵߳ļ���TcpConnection���������״̬ ������̷߳��ʹ��������̰߳�ȫ����
    std::weak_ptr<void> tie_; // ��ֹ�������ֶ�����remove channel�Ժ���ʹ��channel
    bool tied_;

    // �ص���������
    // ��Ϊchannelͨ�������ܹ���֪fd���շ����ľ����¼�revents��������������þ����¼��Ļص�����
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;

    //debug��
    bool eventHandling_; // ����ȷ����channel������ʱ��channel��ִ�еĻص��Ѿ�����
    bool addedToLoop_;  // ����ȷ���ڵ�ǰ��channel������ʱ��Ӧ����ǰ��channel remove���� һ��channelҪ��remove�����������
    
};