#include "Thread.h"
#include "CurrentThread.h"

#include <semaphore.h>

//��̬��Ա����Ҫ�����ⵥ����ʼ��
std::atomic_int Thread::numCreated_ = {0};

Thread::Thread(ThreadFunc func, const std::string& name)
    : started_(false)
    , joined_(false)
    , tid_(0)
    , func_(std::move(func))
    , name_(name) // ����ָ��Ĭ�Ϲ��������
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
    // ֻ�е��߳����������˲���Ҫ�����յĶ���
    // �߳�ֻ�ܹ�����һ��ģʽ�� Ҫô����ͨ�Ĺ����߳� ʹ��join������Դ
    // Ҫôʹ��detach ���̱߳���ػ��߳� ϵͳ���Զ�������Դ
    if (started_ &&!joined_)// detach �� join��mutually exclusive�� 
    {
        thread_->detach();// thread���ṩ�����÷����̵߳ķ���
    }

}

void Thread::start()// һ��Thread�����¼�ľ���һ�����̵߳���ϸ��Ϣ
{
    started_ = true;
    // ���߳�
    // ����һ���ź���������
    sem_t sem;
    sem_init(&sem, false, 0);
    // ����ָ��ָ���̶߳���ʵ�� �������߳�
    // shared_ptr û�н����ⲿ�����������ݸ�ֵ�Ĺ��� ����ұ�Ҫ����std::shared_ptr
    // lambda ���ʽ ��Ϊ�̺߳��� �����õķ�ʽ�����ⲿ����
    thread_ = std::shared_ptr<std::thread>(new std::thread([&](){
        // ���߳�
        //��ȡ�̵߳�tidֵ
        tid_ = CurrentThread::tid();
        sem_post(&sem);// �ź�����Դ+1
    
        // ����һ�����߳�ִ�и��̺߳��� �̺߳����ڳ�Ա�����м�¼��
        func_(); // ��ʵ������ǿ���һ��loop
    }));

    // �������ź�����sem_post�����û����Դ=0�Ļ� ��û��+1�Ļ�
    // ����sem_wait����ѵ�ǰ�����̵߳�start������ס
    // �ȴ����̸߳��ź�����Դ+1 ���ܵ�sem_post˵�����̵߳�tid_ֵ�Ѿ���ȡ���� ����start�߳̾Ϳ��Լ���ִ��
    // �������ȴ���ȡ�����´����̵߳�tidֵ �����ȴ�
    sem_wait(&sem); // �ź���-1
}

void Thread::join()
{
    joined_ = true;
    thread_->join();
}

