#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>
//��װsocket��ַ����
class InetAddress
{
public:
    //Ĭ��ֵ��localhost
    //c++�﷨��������Ĭ��ֵ���������������Ƕ����� ֻ�ܳ���һ��
    explicit InetAddress(uint16_t port=0, std::string ip = "127.0.0.1"); 
    explicit InetAddress(const sockaddr_in& addr)
        : addr_(addr)
    {}

    std::string toIp() const; //��ȡIp
    std::string toIpPort() const; //��ȡIpPort��Ϣ
    uint16_t toPort() const; //��ȡ�˿ں���Ϣ

    const sockaddr_in* getSockAddr() const { return &addr_; } //��ȡ��Ա����
    void setSockAddr(const sockaddr_in &addr) { addr_ = addr; };

private:
    sockaddr_in addr_;
};