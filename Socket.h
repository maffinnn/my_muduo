#pragma once

#include "noncopyable.h"

class InetAddress;

// ��װsocket fd
class Socket : noncopyable
{
public:
    // explicit ��ֹ��ʽ������ʱ����
    explicit Socket(int sockfd)
        : sockfd_(sockfd)
    { }

    ~Socket();

    // ֻ���ӿ�
    int get_fd() const { return sockfd_; }
    void bindAddress(const InetAddress& localaddr);
    void listen();
    int accept(InetAddress* peeraddr);
    void shutdownWrite();

    void setTcpNoDelay(bool on); // �������ݲ�����tcp����
    void setReuseAddr(bool on);
    void setReusePort(bool on);
    void setKeepAlive(bool on);

private:
    const int sockfd_;


};