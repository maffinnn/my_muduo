#include "Thread.h"
#include "CurrentThread.h"

#include <semaphore.h>

//静态成员变量要在类外单独初始化
std::atomic_int Thread::numCreated_ = {0};

Thread::Thread(ThreadFunc func, const std::string& name)
    : started_(false)
    , joined_(false)
    , tid_(0)
    , func_(std::move(func))
    , name_(name) // 智能指针默认构造就行了
{
    setDefaultName();
}
void Thread::setDefaultName()
{
    int num = ++numCreated_; // 
    if (name_.empty())
    {
        char buf[32] = {0};
        snprintf(buf, sizeof buf, "Thread%d",num);
        name_ = buf;
    }
}

Thread::~Thread()
{
    // 只有当线程运行起来了才需要做回收的动作
    // 线程只能工作在一种模式下 要么是普通的工作线程 使用join回收资源
    // 要么使用detach 让线程编程守护线程 系统会自动回收资源
    if (started_ &&!joined_)// detach 和 join是mutually exclusive的 
    {
        thread_->detach();// thread类提供的设置分离线程的方法
    }

}

void Thread::start()// 一个Thread对象记录的就是一个新线程的详细信息
{
    started_ = true;
    // 主线程
    // 定义一个信号量并开启
    sem_t sem;
    sem_init(&sem, false, 0);
    // 智能指针指向线程对象实例 开启子线程
    // shared_ptr 没有接收外部任意类型数据赋值的功能 因此右边要加上std::shared_ptr
    // lambda 表达式 作为线程函数 以引用的方式接收外部对象
    thread_ = std::shared_ptr<std::thread>(new std::thread([&](){
        // 子线程
        //获取线程的tid值
        tid_ = CurrentThread::tid();
        sem_post(&sem);// 信号量资源+1
    
        // 开启一个新线程执行该线程函数 线程函数在成员变量中记录着
        func_(); // 其实这里就是开启一个loop
    }));

    // 如果这个信号量在sem_post那里边没有资源=0的话 即没有+1的话
    // 会在sem_wait这里把当前的主线程的start给阻塞住
    // 等待子线程给信号量资源+1 能跑到sem_post说明子线程的tid_值已经获取到了 那样start线程就可以继续执行
    // 这里必须等待获取上面新创建线程的tid值 阻塞等待
    sem_wait(&sem); // 信号量-1
}

void Thread::join()
{
    joined_ = true;
    thread_->join();
}

