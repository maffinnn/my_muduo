#include "TcpServer.h"
#include "Logger.h"

#include <functional>
#include <string>
#include <strings.h> // bzero



EventLoop* CheckLoopNotNULL(EventLoop* loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d mainLoop is null \n", __FILE__, __FUNCTION__,__LINE__);
    }
    return loop;
}

// ������һ��TcpServer�����ͬʱ������acceptor��poller
TcpServer::TcpServer(EventLoop* loop, 
                const InetAddress& listenAddr, 
                const std::string& nameArg,
                Option option)
     : loop_(CheckLoopNotNULL(loop))
     , ipPort_(listenAddr.toIpPort())
     , name_(nameArg)
     , acceptor_(new Acceptor(loop, listenAddr, option == kReusePort))
     , threadPool_(new EventLoopThreadPool(loop, name_))
     , connectionCallback_(defaultConnectionCallback)
     , messageCallback_(defaultMessageCallback)
     , nextConnId_(1)
{
    // �������û�����ʱ ��ִ��TcpServer::newConnection��ΪĬ�ϻص�
    acceptor_->setNewConnectionCallback(
            std::bind(&TcpServer::newConnection, this, 
            std::placeholders::_1, std::placeholders::_2));
    
}

TcpServer::~TcpServer()
{
    LOG_INFO("TcpServer::~TcpServer [%s] destructing\n", name_.c_str());

    for(auto& item : connections_)
    {
        TcpConnectionPtr conn(item.second); // ��һ���ֲ�������ָ��ָ�����TcpConnection���� �ô��ǳ����һ����žͻ��Զ��ͷ�new��������Դ ������Ҫ��Ҫ��ȡ��Ӧ��subloop
        item.second.reset(); // TcpServer��������ǿ����ָ�벻����ָ�����TcpConnection������reset�൱�ڲ��������ǿ����ָ��ȥ������Դ�� �����ͷ���Դ��
        conn->getLoop()->runInLoop(
            std::bind(&TcpConnection::connectDestroyed, conn));
    }
}
 

// ���õײ�subloop�ĸ���
void TcpServer::setThreadNum(int numThreads)
{
    threadPool_->setThreadNum(numThreads);
}

void TcpServer::start()
{
    // ԭ�Ӳ��� ��ֹһ��TcpServer����start��� 
    if (started_++ == 0)// ��Ϊ�տ�ʼstarted_Ϊ0�������� ++�� ����ظ�����Tcp start �Ͳ����������� 
    {
        threadPool_->start(threadInitCallback_);// �����ײ��loop�̳߳�
        // ����acceptor�������� loop_��mainLoop
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));// acceptor_.get ��ȡһ����ָ��

    }
}


// ��һ���¿ͻ��˵����� acceptor��ִ������ص�����
void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr)
{
    // ��ѯ�㷨 ѡ��һ��subloop������channel
    EventLoop* ioLoop = threadPool_->getNextLoop();
    char buf[64] = {0};
    snprintf(buf, sizeof(buf), "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;// nextConnId �������û�ж����ԭ�Ӳ�����Ϊֻ��һ��mainloop�ٴ���������� û���̰߳�ȫ���� ���û�б�Ҫ�����ԭ�Ӳ���
    std::string connName = name_ + buf;

    LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from %s \n",
        name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str());
    // ͨ��sockfd��ȡ��󶨵ı��ص�ip��ַ���˿���Ϣ
    sockaddr_in local;
    bzero(&local, sizeof local);
    socklen_t addrlen = sizeof local;
    if (::getsockname(sockfd, (sockaddr*)&local, &addrlen)<0)
    {
        LOG_ERROR("socket::getLocalAddr");
    }
    InetAddress localAddr(local);

    // �������ӳɹ���sockfd ����TcpConnection
    TcpConnectionPtr conn(new TcpConnection(ioLoop, 
                                            connName,
                                            sockfd, // ���channel
                                            localAddr,
                                            peerAddr));

    connections_[connName] = conn;
    // ����Ļص������û����ø�TcpServer TcpServer���ø�TcpConnection 
    // TcpConnection���ø�channel channel��ע�ᵽpoller�� 
    // poller���֪ͨ��notify��channelִ�лص�
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    // ��������ιر����ӵĻص� con->shutdown()
    conn->setCloseCallback(
        std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));
    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));

}
void TcpServer::removeConnection(const TcpConnectionPtr& conn)
{
    loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));

}
void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn)
{
    LOG_INFO("TcpServer::removeConnectionInLoop [%s] -connection %s \n", name_.c_str(), conn->name().c_str());

    size_t n = connections_.erase(conn->name());
    EventLoop* ioLoop = conn->getLoop();
    ioLoop->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));

}
