
#include "Timestamp.h"

#include <time.h>

Timestamp::Timestamp()
    :microSecondsSinceEpoch_(0)
{}
    
Timestamp::Timestamp(int64_t microSecondsSinceEpoch)
    :microSecondsSinceEpoch_(microSecondsSinceEpoch)
{}
    
//��ȡ��ǰʱ��
Timestamp Timestamp::now()
{
    return Timestamp(time(NULL));
}

//��ȡ��ʽ���ַ������ʱ��
//����const �������޸ĳ�Ա������ֵ ->ֻ������
std::string Timestamp::toString() const
{
    char buf[128] = {0};
    tm* tm_time = localtime(&microSecondsSinceEpoch_);
    //��/��/�� ʱ/��/��
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