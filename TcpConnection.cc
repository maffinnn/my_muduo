#include "TcpConnection.h"
#include "Logger.h"
#include "Socket.h"
#include "Channel.h"
#include "InetAddress.h"
#include "EventLoop.h"

#include <functional>
#include <errno.h>
#include <cassert>
#include <unistd.h>
#include <string>

EventLoop *CheckLoopNotNULL(EventLoop *loop);

TcpConnection::TcpConnection(EventLoop *loop,
                             const std::string &nameArg,
                             int sockfd,
                             const InetAddress &localAddr,
                             const InetAddress &peerAddr)
    : loop_(CheckLoopNotNULL(loop)), name_(nameArg), state_(kConnecting), reading_(true), socket_(new Socket(sockfd)), channel_(new Channel(loop, sockfd)), localAddr_(localAddr), peerAddr_(peerAddr), highWaterMark_(64 * 1024 * 1024) // 64M
{
        channel_->setReadCallback(std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
        channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
        channel_->setCloseCallback(std::bind(&TcpConnection::handleClose, this));
        channel_->setErrorCallback(std::bind(&TcpConnection::handleError, this));
        LOG_INFO("TcpConnection::ctor[%s] at fd=%d\n", name_.c_str(), sockfd);
        socket_->setKeepAlive(true); // !!! Keepalive机制
}

TcpConnection::~TcpConnection()
{
        // TcpConnection没有自己开辟堆上资源出了两个智能指针 会自动释放
        // 因此析构我们只要做个打印
        LOG_INFO("TcpConnection::ctor[%s] at fd=%d state=%d \n", name_.c_str(), channel_->get_fd(), (int)state_);
}

void TcpConnection::handleRead(Timestamp receiveTime)
{
        int savedErrno = 0;
        ssize_t n = inputBuffer_.readFd(channel_->get_fd(), &savedErrno);
        // 有数据了
        if (n > 0)
        {
                // 已建立连接的用户 有可读事件发生了， 调用用户传入的回调操作
                // TcpConnection继承自 enable_shared_from_this 这里shared_from_this returns a shared_ptr which shared the ownership of *this
                messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
        }
        else if (n == 0)
        {
                // 客户端断开
                handleClose();
        }
        else
        {
                // 出错
                errno = savedErrno;
                LOG_ERROR("TcpConnction::handleRead");
                handleError();
        }
}

void TcpConnection::handleWrite()
{
        int savedErrno = 0;
        if (channel_->isWriting())
        {
                ssize_t n = outputBuffer_.writeFd(channel_->get_fd(), &savedErrno);
                if (n > 0)
                {
                        // 有数据
                        outputBuffer_.retrieve(n);
                        if (outputBuffer_.readableBytes() == 0)
                        {
                                // 表示已经发送完成了
                                channel_->disableWriting();
                                if (writeCompleteCallback_)
                                {
                                        // 唤醒这个loop_对应的thread线程 执行回调
                                        loop_->queueInLoop(
                                            std::bind(writeCompleteCallback_, shared_from_this()));
                                }
                                if (state_ == kDisconnecting)
                                {
                                        shutdownInLoop();
                                }
                        }
                }
                else
                {
                        LOG_ERROR("TcpConncection::handleWrite");
                }
        }
        else
        {
                LOG_ERROR("TcpConnection fd=%d is down, no more writing\n", channel_->get_fd());
        }
}

void TcpConnection::handleClose()
{
        LOG_INFO("fd=%d state=%d \n", channel_->get_fd(), (int)state_);
        assert(state_ == kConnected || state_ == kDisconnecting);
        //we dont close fd, leave it to destrctor, so we can find leaks easily
        setState(kDisconnected);
        channel_->disableAll(); // 对所有的事件都不感兴趣了

        TcpConnectionPtr connPtr(shared_from_this());
        connectionCallback_(connPtr); // 执行连接关闭的回调
        // must be the last line
        closeCallback_(connPtr);// 关闭连接回调 执行的是TcpServer::remvoeConnection方法
}
void TcpConnection::handleError()
{
        int optval;
        socklen_t optlen = sizeof(optval);
        int err = 0;
        if (::getsockopt(channel_->get_fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
        {
                err = errno;
        }
        else
        {
                err = optval;
        }
        LOG_ERROR("TcpConnection::handleError name:%s - SO_ERROR:%d \n", name_.c_str(), err);
}

void TcpConnection::send(const std::string &message)
{
        if (state_ == kConnected)
        {
                if (loop_->isInLoopThread())
                {
                        sendInLoop(message.c_str(), message.size());
                }
                else
                {
                        loop_->runInLoop(std::bind(&TcpConnection::sendInLoop, this, message.c_str(), message.size()));
                }
        }
}

// 发送数据 应用写得快（非阻塞io）而内核发送数据慢 需要把带发送数据写入缓冲区 而且设置了水位回调
void TcpConnection::sendInLoop(const void *data, size_t len)
{
        ssize_t nwrote = 0;
        size_t remaining = len; // 剩下的data部分
        bool faultError = false;

        // 之前已经调用过该connection的shutdown  不能再进行发送了
        if (state_ == kDisconnected)
        {
                LOG_ERROR("disconneted, give up writing\n");
                return;
        }
        // 如果peer端没有写事件发生且 本机的缓冲区没有带发送数据
        // if nothing in output queue， try writing directly
        if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
        {
                nwrote = ::write(channel_->get_fd(), data, len);
                if (nwrote >= 0)
                {
                        // 写成功
                        remaining = len - nwrote;
                        // 刚好一次性写完
                        if (remaining == 0 && writeCompleteCallback_)
                        {
                                // 既然在这里数据全部发送完成， 就不用再给channel设置EPOLLOUT事件了
                                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
                        }
                }
                else //nwrote<0 出错
                {
                        nwrote = 0;
                        if (errno != EWOULDBLOCK)
                        {
                                LOG_ERROR("TcpConnection::sendInLoop");
                                if (errno == EPIPE || errno == ECONNRESET) // 接收到对端的socket连接的重置
                                {
                                        faultError = true;
                                }
                        }
                }
        }
        // 说明当前这一次write并没有把数据全部发送出去， 剩余的数据需要保存到缓冲区当中去
        // 然后给channel注册epollout事件 poller发现tcp的发送缓冲区有空间 会通知相应的sock-channel， 调用writeCompleteCallback_回调方法
        // 就会回调相应的TcpCOnnection::handleWrite方法， 把发送缓冲区中的数据全部发送完成
        if (!faultError && remaining > 0)
        {
                // 目前发送缓冲区剩余的待发送的长度
                size_t oldLen = outputBuffer_.readableBytes();
                if (oldLen + remaining >= highWaterMark_ && oldLen < highWaterMark_ && highWaterMarkCallback_) // oldLen之前是小于highWaterMark_ 与剩下没写完的data数据相加后大于了水位线 那么就回调水位线处理函数
                {
                        loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));
                }
                // 将待发送数据继续添加到缓冲区当中
                outputBuffer_.append(static_cast<const char *>(data) + nwrote, remaining);
                if (!channel_->isWriting())
                {
                        // 这里一定要注册channel的写事件 否则poller不会给channel通知epollout
                        // 不通知epollout就没有办法让poller去驱动channel调用它writecallback 也就不能调用tcpconnection的handlewrite
                        channel_->enableWriting();
                }
        }
}

// 连接建立
void TcpConnection::connectEstablished()
{
        setState(kConnected);
        // TcpConnection 对象绑定一个channel 如果没有绑定则channel之后回调TcpConnection里的操作就会出问题 找不到this指针
        channel_->tie(shared_from_this());
        channel_->enableReading(); // 向poller注册channel的pollin事件
        // 新链接建立了 执行回调
        connectionCallback_(shared_from_this());

}
// 连接销毁
void TcpConnection::connectDestroyed()
{
        if (state_ == kDisconnected)
        {
                setState(kDisconnected);
                channel_->disableAll(); // 把channel的所有感兴趣事件从poller中delete掉
                connectionCallback_(shared_from_this());

        }
        channel_->remove(); // 把channel从poller中删除掉
}

void TcpConnection::shutdown()
{
        if (state_ == kConnected)
        {
                setState(kDisconnecting);
                loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
        }
}

// shutdown是用户调用的 但是此时写操作可能还没有执行完毕 需要判断
void TcpConnection::shutdownInLoop()
{
        if (!channel_->isWriting())// 说明当前outputBuffer_中的数据已经全部发送完成
        {
                // we are not writing
                socket_->shutdownWrite();//关闭写端
        }

}