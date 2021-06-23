#include "InetAddress.h"

#include <string.h>

InetAddress::InetAddress(uint16_t port, std::string ip)
{
    bzero(&addr_, sizeof addr_);
    addr_.sin_family = AF_INET;//ָ����ַ����that your socket wants to communicate to
    addr_.sin_port = htons(port);
    addr_.sin_addr.s_addr = inet_addr(ip.c_str());
}
//��ȡIp
std::string InetAddress::toIp() const
{
    //ע�⹹���ʱ��addr_���Ѿ�ת���������ֽ���Big Endian��
    //���Ҫת�ɱ����ֽ���
    char buf[64] = {0};
    //:: scope operator
    //XXClass::function()������ XXClass��������
    //ǰ��ֻ��::����ȫ��,function()ǰʲô��û��˵���ǵ��õĵ�ǰ��ĳ�Ա������
    //�������ֽ���ת�ɱ����ֽ���
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf);
    return buf;

}
//��ȡIpPort��Ϣ 
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
//��ȡ�˿ں���Ϣ
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