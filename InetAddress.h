#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>
//封装socket地址类型
class InetAddress
{
public:
    //默认值是localhost
    //c++语法：参数的默认值不管是在声明还是定义中 只能出现一次
    explicit InetAddress(uint16_t port=0, std::string ip = "127.0.0.1"); 
    explicit InetAddress(const sockaddr_in& addr)
        : addr_(addr)
    {}

    std::string toIp() const; //获取Ip
    std::string toIpPort() const; //获取IpPort信息
    uint16_t toPort() const; //获取端口号信息

    const sockaddr_in* getSockAddr() const { return &addr_; } //获取成员变量
    void setSockAddr(const sockaddr_in &addr) { addr_ = addr; };

private:
    sockaddr_in addr_;
};