#pragma once

#include "noncopyable.h"

#include <functional>
#include <thread>
#include <memory>
#include <unistd.h>
#include <string>
#include <atomic>

class Thread : noncopyable
{
public:
    // 线程的入口函数
    using ThreadFunc = std::function<void()>; // 注意 为什么线程函数是void()：没有返回值且不带参数 
                                              // 那线程想带参数怎么办 => 可以使用绑定器 与函数对象
    explicit Thread(ThreadFunc, const std::string& name = std::string());
    ~Thread();

    void start();
    void join();

    bool started() const { return started_; }
    pid_t tid() const { return tid_;}
    const std::string& name() const { return name_; }
    static int numCreated() { return numCreated_; }

private:
    void setDefaultName();

    bool started_;
    bool joined_;
    // 不可以直接定义std::thread thread_; 这样定义的话当new Thread时就会直接启动一个线程
    // 我们需要掌控线程启动的时机
    // 使用一个智能指针指向这个std::thread
    std::shared_ptr<std::thread> thread_;
    pid_t tid_;
    ThreadFunc func_;
    std::string name_;
    // CountdownLatch latch_; 可以使用semaphore（信号量）取得相同的效果
    static std::atomic_int numCreated_; // 记录生成的线程数量

};