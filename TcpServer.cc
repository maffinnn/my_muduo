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

// 创建了一个TcpServer对象就同时创建了acceptor和poller
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
    // 当有新用户连接时 会执行TcpServer::newConnection作为默认回调
    acceptor_->setNewConnectionCallback(
            std::bind(&TcpServer::newConnection, this, 
            std::placeholders::_1, std::placeholders::_2));
    
}

TcpServer::~TcpServer()
{
    LOG_INFO("TcpServer::~TcpServer [%s] destructing\n", name_.c_str());

    for(auto& item : connections_)
    {
        TcpConnectionPtr conn(item.second); // 让一个局部的智能指针指向这个TcpConnection对象 好处是出了右花括号就会自动释放new出来的资源 而且需要需要获取相应的subloop
        item.second.reset(); // TcpServer本身的这个强智能指针不会再指向这个TcpConnection对象了reset相当于不再用这个强智能指针去管理资源了 可以释放资源了
        conn->getLoop()->runInLoop(
            std::bind(&TcpConnection::connectDestroyed, conn));
    }
}
 

// 设置底层subloop的个数
void TcpServer::setThreadNum(int numThreads)
{
    threadPool_->setThreadNum(numThreads);
}

void TcpServer::start()
{
    // 原子操作 防止一个TcpServer对象被start多次 
    if (started_++ == 0)// 因为刚开始started_为0条件满足 ++后 如果重复调用Tcp start 就不符合条件了 
    {
        threadPool_->start(threadInitCallback_);// 启动底层的loop线程池
        // 启动acceptor开启监听 loop_是mainLoop
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));// acceptor_.get 获取一个裸指针

    }
}


// 有一个新客户端的连接 acceptor会执行这个回调操作
void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr)
{
    // 轮询算法 选择一个subloop来管理channel
    EventLoop* ioLoop = threadPool_->getNextLoop();
    char buf[64] = {0};
    snprintf(buf, sizeof(buf), "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;// nextConnId 这个变量没有定义成原子操作因为只有一个mainloop再处理这个变量 没有线程安全问题 因此没有必要定义成原子操作
    std::string connName = name_ + buf;

    LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from %s \n",
        name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str());
    // 通过sockfd获取其绑定的本地的ip地址及端口信息
    sockaddr_in local;
    bzero(&local, sizeof local);
    socklen_t addrlen = sizeof local;
    if (::getsockname(sockfd, (sockaddr*)&local, &addrlen)<0)
    {
        LOG_ERROR("socket::getLocalAddr");
    }
    InetAddress localAddr(local);

    // 根据连接成功的sockfd 创建TcpConnection
    TcpConnectionPtr conn(new TcpConnection(ioLoop, 
                                            connName,
                                            sockfd, // 打包channel
                                            localAddr,
                                            peerAddr));

    connections_[connName] = conn;
    // 下面的回调都是用户设置给TcpServer TcpServer设置给TcpConnection 
    // TcpConnection设置给channel channel被注册到poller上 
    // poller最后通知（notify）channel执行回调
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    // 设置了如何关闭连接的回调 con->shutdown()
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
