#pragma once

#include <iostream>
#include <string>

class Timestamp
{
public:
    Timestamp();
    //如果不加explicit就意味着这个Timestamp类型支持与int64_t这个类型的隐式转换的
    //防止程序运行时发生预想不到的错误
    explicit Timestamp(int64_t microSecondsSinceEpoch);
    //获取当前时间
    static Timestamp now();
    //获取格式化字符串后的时间
    //加了const 不允许修改成员变量的值 ->只读方法
    std::string toString() const;
private:
    int64_t microSecondsSinceEpoch_;
};