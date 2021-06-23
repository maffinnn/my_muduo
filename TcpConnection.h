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

// TcpServer ͨ��Accceptor��ȡ���û����� ͨ��accept�����õ�connfd
// �ٽ�connfd�����TcpConnection ��TcpConnection���ûص�����channel
// enable_shared_from_this allows an object t that is currently managed by a std::shared_ptr named pt0 to
// to safely generate additional std::shared_ptr instances pt1, pt2, ...  that all share ownership of t with pt0
class TcpConnection:noncopyable, 
                    public std::enable_shared_from_this<TcpConnection> // �õ���ǰ���������ָ��
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
    void shutdown();// �رյ�ǰ���� �ͻ��˶Ͽ����ڷ������һ��TcpConnection������������ӵ� ��Ҫ��������
    void setConnectionCallback(const ConnectionCallback& cb){ connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback& cb){ messageCallback_ = cb; }
    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark) { highWaterMarkCallback_ = cb; highWaterMark_ = highWaterMark; }
    void setWriteCompleteCallback(const WriteCompleteCallback& cb){ writeCompleteCallback_ = cb; }
    void setCloseCallback(const CloseCallback& cb) { closeCallback_ = cb; }
    // ���ӽ���
    void connectEstablished();
    // ��������
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
    EventLoop* loop_; // ���ﲻ��baseLoop ��ΪTcpConnection������subloop��������
    const std::string name_;
    std::atomic_int state_;
    bool reading_;
    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    ConnectionCallback connectionCallback_; // tcpServer�Ӹ�TcpConnection ����TcpConnection����Channel Ȼ��poller����channel����poller���ûص�
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    CloseCallback closeCallback_;
    HighWaterMarkCallback highWaterMarkCallback_;

    size_t highWaterMark_; // ˮλ��
    Buffer inputBuffer_;
    Buffer outputBuffer_;




};