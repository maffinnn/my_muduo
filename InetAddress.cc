#include "InetAddress.h"

#include <string.h>

InetAddress::InetAddress(uint16_t port, std::string ip)
{
    bzero(&addr_, sizeof addr_);
    addr_.sin_family = AF_INET;//指定地址家族that your socket wants to communicate to
    addr_.sin_port = htons(port);
    addr_.sin_addr.s_addr = inet_addr(ip.c_str());
}
//获取Ip
std::string InetAddress::toIp() const
{
    //注意构造的时候addr_就已经转成了网络字节序（Big Endian）
    //因此要转成本地字节序
    char buf[64] = {0};
    //:: scope operator
    //XXClass::function()代表是 XXClass的作用域
    //前面只有::代表全局,function()前什么都没有说明是调用的当前类的成员函数。
    //从网络字节序转成本地字节序
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf);
    return buf;

}
//获取IpPort信息 
std::string InetAddress::toIpPort() const
{
    //ip:port
    char buf[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf);
    size_t end = strlen(buf);
    uint16_t port = ntohs(addr_.sin_port);
    snprintf(buf+end, (sizeof buf+end+1), ":%u",port);
    return buf;
}
//获取端口号信息
uint16_t InetAddress::toPort() const
{
    return ntohs(addr_.sin_port);
}


// #include <iostream>
// int main()
// {
//     InetAddress addr(8080);
//     std::cout << addr.toIpPort() << std::endl;
//     return 0;
// }