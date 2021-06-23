#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"

#include <cassert>
#include <sys/poll.h>

const int Channel::kNoneEvent = 0; //û���¼�
//����poll�ļ�������epoll�ĺ��ֵ����ȫһ���ұ�����˼Ҳ��һ���� ���Լ�����EPOLL Ҳ������POLL
//muduo����ͬʱ֧��poll��epoll�� �������߳������poller�����
//POLLIN -- there is data to read 
//POLLPRI -- there is urgent data to read
//POLLOUT -- writing will not be blocked
const int Channel::kReadEvent = POLLIN | POLLPRI;
const int Channel::kWriteEvent = POLLOUT;

//EventLoop����һ��ChannelList��һ��poller
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
    assert(!eventHandling_);// ��֤channel�ص��ѷ��� û���ڲ����ص�
    assert(!addedToLoop_); // ��֤channelҪô����û�б�ʹ�ù� Ҫô�Ѿ���removed���� ��������������

    // �̺߳��¼�ѭ����һһ��Ӧ�Ĺ�ϵ
    // ��仰����˼���жϵ�ǰҪ������channel�Ƿ����������ڵ�eventLoop���� ���ڵ�ǰ�¼�ѭ�����ڵ��߳��ﱻ����
    // if(loop_->isInLoopThread())
    // {
    //     assert(!loop_->hasChannel(this));
    // }

}

//Channel��tie����
// TcpConnection������һ��channel
void Channel::tie(const std::shared_ptr<void>& obj)
{
    //std::weak_ptr<void> tie_ ��һ��������ָ�� ���۲��� ���ڹ۲�һ��ǿ����ָ��obj
    tie_ = obj;
    tied_ = true;
}

// �����ǵ��ı�channel����ʾfd��events�¼��� ʹ��update��poller���������Ӧ���¼�epoll_ctl
void Channel::update()
{
    // ͨ��channel������eventloop����poller����Ӧ������ע��fd��events�¼�
    addedToLoop_ = true;
    loop_->updateChannel(this); //this�ǵ�ǰchannel
}

// ��channel������eventloop�е���remove��channelɾ��
void Channel::remove()
{
    assert(isNoneEvent());
    addedToLoop_ = false;
    loop_->removeChannel(this);
}

// fd�õ�poller֪ͨ�Ժ� �����¼�
void Channel::handleEvent(Timestamp receiveTime)
{
    if(tied_) // �ж��Ƿ�󶨹�
    {
        //��Ա���� tie������һ����ǰ�����channel
         std::shared_ptr<void> guard = tie_.lock();// ��weak_ptr������ǿ����ָ�� 
        //�ж���Ӧ�ĵ�ǰ��channel�Ƿ���
        if(guard)
        {
            handleEventWithGuard(receiveTime);                                                                                                                                                                                                                                                                                                                                                                     
        }
    }
    else
    {
        //�������ʧ�� ��δ��tie_�� ��TcpConnection��remove����
        handleEventWithGuard(receiveTime);
    }
}
//POLLHUP -- Hang up output only(==only used to indicate the status of revents)
//POLLNVAL -- Invalid request:fd not open output only(==only used to indicate the status of revents)
// ����poller֪ͨ��channel�����ľ����¼�����channel���������Ӧ�Ļص�����
void Channel::handleEventWithGuard(Timestamp receiveTime)
{
    eventHandling_ = true;
    LOG_INFO("channel handleEvent revents:%d\n",revents_);
    //�����revents���ص��ǹ���״̬ �����޷���ȡ
    //�����ر��쳣
    if ((revents_ & POLLHUP)&&!(revents_&POLLIN))
    {
        //���closeCallback��Ϊ��
        if(closeCallback_) closeCallback_();

    }
    // �쳣
    if(revents_ & (POLLERR|POLLNVAL))
    {
        if (errorCallback_) errorCallback_();
    }
    //�ж��¼�
    if (revents_ & POLLIN | POLLPRI | POLLRDHUP)
    {
        if (readCallback_) readCallback_(receiveTime);
    }
    //��д�¼�
    if (revents_ & POLLOUT)
    {
        if (writeCallback_) writeCallback_();
    }

    eventHandling_ = false;
}