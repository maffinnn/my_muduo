#pragma once

#include "noncopyable.h"
#include "InetAddress.h"
#include "Buffer.h"
#include "Callbacks.h"
#include "Timestamp.h"

#include <memory>
#include <string>
#include <atomic>

class Channel;
class EventLoop;
class Socket;

// TcpServer 通过Accceptor获取新用户链接 通过accept函数拿到connfd
// 再将connfd打包给TcpConnection 由TcpConnection设置回调交给channel
// enable_shared_from_this allows an object t that is currently managed by a std::shared_ptr named pt0 to
// to safely generate additional std::shared_ptr instances pt1, pt2, ...  that all share ownership of t with pt0
class TcpConnection:noncopyable, 
                    public std::enable_shared_from_this<TcpConnection> // 得到当前对象的智能指针
{
public:
    TcpConnection(EventLoop* loop, 
                const std::string &name,
                int sockfd,
                const InetAddress& localAddr,
                const InetAddress& peerAddr);

    ~TcpConnection();

    EventLoop* getLoop() const { return loop_;}
    const std::string& name() const { return name_; }
    const InetAddress& localAddress() const { return localAddr_; }
    const InetAddress& peerAddress() const { return peerAddr_; }
    bool connected() const { return state_ == kConnected; }
    bool disconnected() const {return state_ == kDisconnected; }

    void send(const std::string& message);
    void shutdown();// 关闭当前连接 客户端断开了在服务端有一个TcpConnection是描述这个连接的 需要把它回收
    void setConnectionCallback(const ConnectionCallback& cb){ connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback& cb){ messageCallback_ = cb; }
    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark) { highWaterMarkCallback_ = cb; highWaterMark_ = highWaterMark; }
    void setWriteCompleteCallback(const WriteCompleteCallback& cb){ writeCompleteCallback_ = cb; }
    void setCloseCallback(const CloseCallback& cb) { closeCallback_ = cb; }
    // 连接建立
    void connectEstablished();
    // 连接销毁
    void connectDestroyed();

    

private:
    enum StateE { kDisconnected, kConnecting, kConnected, kDisconnecting };
    void setState(StateE state){ state_ = state; }
    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(const void* message, size_t len);
    void shutdownInLoop();
    EventLoop* loop_; // 这里不是baseLoop 因为TcpConnection都是在subloop里面管理的
    const std::string name_;
    std::atomic_int state_;
    bool reading_;
    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    ConnectionCallback connectionCallback_; // tcpServer扔给TcpConnection 再由TcpConnection交给Channel 然后poller监听channel再由poller调用回调
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    CloseCallback closeCallback_;
    HighWaterMarkCallback highWaterMarkCallback_;

    size_t highWaterMark_; // 水位线
    Buffer inputBuffer_;
    Buffer outputBuffer_;




};