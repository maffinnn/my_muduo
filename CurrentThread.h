#pragma once

namespace CurrentThread
{
    //__thread (gcc)
    //û��__threadʱ t_cacheTid ��һ��ȫ�ֱ��� �����߳��ǹ����
    //����__thread �Ժ��൱��ÿһ���߳���ӵ�е����������localise to the thread
    //������������ĸ��� ����߳��ǿ�������
    extern __thread int t_cachedTid;

    void cacheTid();

    //����inline??
    inline int tid()
    {
        if (__builtin_expect(t_cachedTid == 0, 0))//���ǽ�cachedTid��ʼ��Ϊ0
        {
            //�����û�л�ȡ��
            cacheTid();
        }
        return t_cachedTid;
    }
    
}