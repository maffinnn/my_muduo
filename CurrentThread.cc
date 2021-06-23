#include "CurrentThread.h"

#include <unistd.h>
#include <sys/syscall.h>

namespace CurrentThread
{
    __thread int t_cacheTid = 0;

    void cacheTid()
    {
        if (t_cachedTid == 0)
        {
            //ͨ��linuxϵͳ���û�ȡ��ǰ�߳�id
            t_cachedTid = static_cast<pid_t>(::syscall(SYS_gettid));
        }
    }
}