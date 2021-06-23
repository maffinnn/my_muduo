#pragma once

#include "noncopyable.h"

class InetAddress;

// 封装socket fd
class Socket : noncopyable
{
public:
    // explicit 防止隐式产生临时对象
    explicit Socket(int sockfd)
        : sockfd_(sockfd)
    { }

    ~Socket();

    // 只读接口
    int get_fd() const { return sockfd_; }
    void bindAddress(const InetAddress& localaddr);
    void listen();
    int accept(InetAddress* peeraddr);
    void shutdownWrite();

    void setTcpNoDelay(bool on); // 对于数据不进行tcp缓冲
    void setReuseAddr(bool on);
    void setReusePort(bool on);
    void setKeepAlive(bool on);

private:
    const int sockfd_;


};