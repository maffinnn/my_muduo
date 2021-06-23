
#pragma once
/*
    noncopyable���̳��Ժ������������������Ĺ�����������������������
    �޷����п�������͸�ֵ����
*/
class noncopyable
{
public:
    noncopyable(const noncopyable&) = delete;
    //���ﷵ��ֵд���û���void����ν�� �Ͼ�delete����
    void operator=(const noncopyable&) = delete;
protected:
    //����Ĺ����������Ҫʵ�ֵ� �Ͼ������๹��ʱ�����
    noncopyable() = default;
    ~noncopyable() = default;
};
