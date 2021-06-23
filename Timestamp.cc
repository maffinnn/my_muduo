
#include "Timestamp.h"

#include <time.h>

Timestamp::Timestamp()
    :microSecondsSinceEpoch_(0)
{}
    
Timestamp::Timestamp(int64_t microSecondsSinceEpoch)
    :microSecondsSinceEpoch_(microSecondsSinceEpoch)
{}
    
//获取当前时间
Timestamp Timestamp::now()
{
    return Timestamp(time(NULL));
}

//获取格式化字符串后的时间
//加了const 不允许修改成员变量的值 ->只读方法
std::string Timestamp::toString() const
{
    char buf[128] = {0};
    tm* tm_time = localtime(&microSecondsSinceEpoch_);
    //年/月/日 时/分/秒
    snprintf(buf, 128, "%4d/%02d/%02d %02d:%02d:%02d", 
        tm_time->tm_year+1900, 
        tm_time->tm_mon+1,
        tm_time->tm_mday,
        tm_time->tm_hour,
        tm_time->tm_min,
        tm_time->tm_sec);

    return buf;
}

// #include <iostream>
// int main()
// {
//     std::cout << Timestamp::now().toString() << std::endl;
//     return 0;
// }  