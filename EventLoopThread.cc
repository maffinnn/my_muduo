#include "EventLoopThread.h"
#include "EventLoop.h"
#include "Thread.h"

EventLoopThread::EventLoopThread(const ThreadInitCallback& cb, const std::string& name)
        : loop_(nullptr)
        , exiting_(false)
        , thread_(std::bind(&EventLoopThread::threadFunc, this), name) // ��û�����������߳� ֻ�ǳ�ʼ����һ���߳����ʵ��
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
        loop_->quit(); // ���߳���󶨵��¼�ѭ���˳�
        thread_.join();
    }
}

// ������������ڸ��߳���
EventLoop* EventLoopThread::startLoop()
{
    // �����ײ����߳� ִ�е��̺߳������ǵ�ǰEventLoop�ڹ������������threadFunc
    thread_.start();

    EventLoop* loop = nullptr;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        // ��loop��û����ʱ �Ǿ�һֱ�ȴ��ڻ������� ֱ���յ������֪ͨ
        while(loop_ == nullptr)
        {
            cond_.wait(lock);
        }
        loop = loop_;
    }
    return loop;// ���ذ󶨵�loop����ĵ�ַ
}

// ��������������ڵ��������߳��������е�
void EventLoopThread::threadFunc()
{
    // ����һ��������EventLoop ��������߳���һһ��Ӧ��
    EventLoop loop;

    if (callback_) // ����û��а�ThreadInit�ص�
    {
        callback_(&loop); // �ͽ�loop����ThreadInitCallback ��һЩ�û��Զ���Ĳ���
    }

    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one();// �õ����loop������ ��֪ͨ��ǰ�߳� �ſ��Է���
    }

    loop.loop(); // �����ײ�EventLoop���¼�ѭ���������� poller.poll

    // ��loop������ �������ĳ���Ҫ�ر��� ���ڽ����¼�ѭ����
    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = nullptr;

}