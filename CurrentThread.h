#pragma once

namespace CurrentThread
{
    //__thread (gcc)
    //没有__thread时 t_cacheTid 是一个全局变量 所有线程是共享的
    //加了__thread 以后相当于每一个线程所拥有的这个变量是localise to the thread
    //对于这个变量的更改 别的线程是看不到的
    extern __thread int t_cachedTid;

    void cacheTid();

    //这里inline??
    inline int tid()
    {
        if (__builtin_expect(t_cachedTid == 0, 0))//我们将cachedTid初始化为0
        {
            //如果还没有获取过
            cacheTid();
        }
        return t_cachedTid;
    }
    
}