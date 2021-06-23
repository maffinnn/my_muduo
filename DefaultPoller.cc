#include "Poller.h"
//#include "PollPoller.h"
#include "EpollPoller.h"

#include <stdlib.h> //getenv()

Poller* Poller::newDefaultPoller(EventLoop* loop)
{
    // if (::getenv("MUDUO_USE_POLL"))
    // {
        //如果在系统环境变量中添加了MUDUO_USE_POLL 生成poll的实例
        //return new PollPoller(loop);
    // }
    //else{
        return new EpollPoller(loop);
    //}
}