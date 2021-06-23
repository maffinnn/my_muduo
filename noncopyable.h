
#pragma once
/*
    noncopyable被继承以后，派生类对象可以正常的构造和析构，但是派生类对象
    无法进行拷贝构造和赋值操作
*/
class noncopyable
{
public:
    noncopyable(const noncopyable&) = delete;
    //这里返回值写引用还是void无所谓了 毕竟delete掉了
    void operator=(const noncopyable&) = delete;
protected:
    //基类的构造和析构需要实现的 毕竟派生类构造时会调用
    noncopyable() = default;
    ~noncopyable() = default;
};
