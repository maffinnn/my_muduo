#pragma once

#include "noncopyable.h"

#include <functional>
#include <string>
#include <vector>
#include <memory>

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    EventLoopThreadPool(EventLoop* baseLoop, const std::string& nameArg);
    ~EventLoopThreadPool();

    void setThreadNum(int numThreads) { numThreads_ = numThreads; }

    void start(const ThreadInitCallback& cb = ThreadInitCallback());

    //valid after calling start()
    // ��������ڶ��߳��� baseLoop_Ĭ������ѯ��round-robin���ķ�ʽ����channel��subloop
    EventLoop* getNextLoop();

    std::vector<EventLoop*> getAllLoops();

    bool isStarted() const { return started_; }
    const std::string& name() const { return name_; }
private:
    EventLoop* baseLoop_; 
    std::string name_;
    bool started_;
    int numThreads_;
    int next_;
    std::vector<std::unique_ptr<EventLoopThread>> threads_; // ���������д����¼����߳�
    std::vector<EventLoop*> loops_; // ���������е��¼��̵߳�loopָ��

};