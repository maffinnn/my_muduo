#include "Poller.h"
//#include "PollPoller.h"
#include "EpollPoller.h"

#include <stdlib.h> //getenv()

Poller* Poller::newDefaultPoller(EventLoop* loop)
{
    // if (::getenv("MUDUO_USE_POLL"))
    // {
        //�����ϵͳ���������������MUDUO_USE_POLL ����poll��ʵ��
        //return new PollPoller(loop);
    // }
    //else{
        return new EpollPoller(loop);
    //}
}