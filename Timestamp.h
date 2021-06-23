#pragma once

#include <iostream>
#include <string>

class Timestamp
{
public:
    Timestamp();
    //�������explicit����ζ�����Timestamp����֧����int64_t������͵���ʽת����
    //��ֹ��������ʱ����Ԥ�벻���Ĵ���
    explicit Timestamp(int64_t microSecondsSinceEpoch);
    //��ȡ��ǰʱ��
    static Timestamp now();
    //��ȡ��ʽ���ַ������ʱ��
    //����const �������޸ĳ�Ա������ֵ ->ֻ������
    std::string toString() const;
private:
    int64_t microSecondsSinceEpoch_;
};