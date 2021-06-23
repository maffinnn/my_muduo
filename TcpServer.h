#pragma once

#include "noncopyable.h"
#include "EventLoop.h"
#include "Acceptor.h"
#include "InetAddress.h"
#include "EventLoopThreadPool.h"
#include "Callbacks.h"
#include "TcpConnection.h"
#include "Timestamp.h"

#include <functional>
#include <string>
#include <atomic>
#include <unordered_map>


class TcpServer : noncopyable
{
public: 
    using ThreadInitCallback = std::function<void(EventLoop*)>;
    
    enum Option
    {
        kNoReusePort,
        kReusePort,
    };
    
    TcpServer(EventLoop* loop,
            const InetAddress& listenAddr,
            const std::string& nameArg,
            Option option = kNoReusePort);
    ~TcpServer();

    void setThreadInitCallback(const ThreadInitCallback& cb){ threadInitCallback_ = cb; }
    void setConnectionCallback(const ConnectionCallback& cb){ connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback& cb){ messageCallback_ = cb; }
    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark) { highWaterMarkCallback_ = cb; highWaterMark_ = highWaterMark; }
    void setWriteCompleteCallback(const WriteCompleteCallback& cb){ writeCompleteCallback_ = cb; }
    
    // ���õײ�subloop�ĸ���
    void setThreadNum(int numThreads);

    void start();


private:
    void newConnection(int sockfd, const InetAddress& peerAddr);
    void removeConnection(const TcpConnectionPtr& conn);
    void removeConnectionInLoop(const TcpConnectionPtr& conn);

    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;
    EventLoop* loop_; // baseLoop �û������loop
    const std::string ipPort_;
    const std::string name_;
    std::unique_ptr<Acceptor> acceptor_; // avoid revealing Acceptor
    std::shared_ptr<EventLoopThreadPool> threadPool_;
    // �û������callbacks
    ConnectionCallback connectionCallback_; // ��������ʱ�Ļص�
    MessageCallback messageCallback_; // �ж�д��Ϣʱ�Ļص�
    WriteCompleteCallback writeCompleteCallback_; // ��Ϣ�������Ժ�Ļص�
    HighWaterMarkCallback highWaterMarkCallback_;

    ThreadInitCallback threadInitCallback_; // �̳߳�ʼ���Ļص�
    std::atomic_int started_;
    int nextConnId_;
    ConnectionMap connections_;// �������е�����
    size_t highWaterMark_;


};