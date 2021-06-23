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
        socket_->setKeepAlive(true); // !!! Keepalive����
}

TcpConnection::~TcpConnection()
{
        // TcpConnectionû���Լ����ٶ�����Դ������������ָ�� ���Զ��ͷ�
        // �����������ֻҪ������ӡ
        LOG_INFO("TcpConnection::ctor[%s] at fd=%d state=%d \n", name_.c_str(), channel_->get_fd(), (int)state_);
}

void TcpConnection::handleRead(Timestamp receiveTime)
{
        int savedErrno = 0;
        ssize_t n = inputBuffer_.readFd(channel_->get_fd(), &savedErrno);
        // ��������
        if (n > 0)
        {
                // �ѽ������ӵ��û� �пɶ��¼������ˣ� �����û�����Ļص�����
                // TcpConnection�̳��� enable_shared_from_this ����shared_from_this returns a shared_ptr which shared the ownership of *this
                messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
        }
        else if (n == 0)
        {
                // �ͻ��˶Ͽ�
                handleClose();
        }
        else
        {
                // ����
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
                        // ������
                        outputBuffer_.retrieve(n);
                        if (outputBuffer_.readableBytes() == 0)
                        {
                                // ��ʾ�Ѿ����������
                                channel_->disableWriting();
                                if (writeCompleteCallback_)
                                {
                                        // �������loop_��Ӧ��thread�߳� ִ�лص�
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
        channel_->disableAll(); // �����е��¼���������Ȥ��

        TcpConnectionPtr connPtr(shared_from_this());
        connectionCallback_(connPtr); // ִ�����ӹرյĻص�
        // must be the last line
        closeCallback_(connPtr);// �ر����ӻص� ִ�е���TcpServer::remvoeConnection����
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

// �������� Ӧ��д�ÿ죨������io�����ں˷��������� ��Ҫ�Ѵ���������д�뻺���� ����������ˮλ�ص�
void TcpConnection::sendInLoop(const void *data, size_t len)
{
        ssize_t nwrote = 0;
        size_t remaining = len; // ʣ�µ�data����
        bool faultError = false;

        // ֮ǰ�Ѿ����ù���connection��shutdown  �����ٽ��з�����
        if (state_ == kDisconnected)
        {
                LOG_ERROR("disconneted, give up writing\n");
                return;
        }
        // ���peer��û��д�¼������� �����Ļ�����û�д���������
        // if nothing in output queue�� try writing directly
        if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
        {
                nwrote = ::write(channel_->get_fd(), data, len);
                if (nwrote >= 0)
                {
                        // д�ɹ�
                        remaining = len - nwrote;
                        // �պ�һ����д��
                        if (remaining == 0 && writeCompleteCallback_)
                        {
                                // ��Ȼ����������ȫ��������ɣ� �Ͳ����ٸ�channel����EPOLLOUT�¼���
                                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
                        }
                }
                else //nwrote<0 ����
                {
                        nwrote = 0;
                        if (errno != EWOULDBLOCK)
                        {
                                LOG_ERROR("TcpConnection::sendInLoop");
                                if (errno == EPIPE || errno == ECONNRESET) // ���յ��Զ˵�socket���ӵ�����
                                {
                                        faultError = true;
                                }
                        }
                }
        }
        // ˵����ǰ��һ��write��û�а�����ȫ�����ͳ�ȥ�� ʣ���������Ҫ���浽����������ȥ
        // Ȼ���channelע��epollout�¼� poller����tcp�ķ��ͻ������пռ� ��֪ͨ��Ӧ��sock-channel�� ����writeCompleteCallback_�ص�����
        // �ͻ�ص���Ӧ��TcpCOnnection::handleWrite������ �ѷ��ͻ������е�����ȫ���������
        if (!faultError && remaining > 0)
        {
                // Ŀǰ���ͻ�����ʣ��Ĵ����͵ĳ���
                size_t oldLen = outputBuffer_.readableBytes();
                if (oldLen + remaining >= highWaterMark_ && oldLen < highWaterMark_ && highWaterMarkCallback_) // oldLen֮ǰ��С��highWaterMark_ ��ʣ��ûд���data������Ӻ������ˮλ�� ��ô�ͻص�ˮλ�ߴ�����
                {
                        loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));
                }
                // �����������ݼ�����ӵ�����������
                outputBuffer_.append(static_cast<const char *>(data) + nwrote, remaining);
                if (!channel_->isWriting())
                {
                        // ����һ��Ҫע��channel��д�¼� ����poller�����channel֪ͨepollout
                        // ��֪ͨepollout��û�а취��pollerȥ����channel������writecallback Ҳ�Ͳ��ܵ���tcpconnection��handlewrite
                        channel_->enableWriting();
                }
        }
}

// ���ӽ���
void TcpConnection::connectEstablished()
{
        setState(kConnected);
        // TcpConnection �����һ��channel ���û�а���channel֮��ص�TcpConnection��Ĳ����ͻ������ �Ҳ���thisָ��
        channel_->tie(shared_from_this());
        channel_->enableReading(); // ��pollerע��channel��pollin�¼�
        // �����ӽ����� ִ�лص�
        connectionCallback_(shared_from_this());

}
// ��������
void TcpConnection::connectDestroyed()
{
        if (state_ == kDisconnected)
        {
                setState(kDisconnected);
                channel_->disableAll(); // ��channel�����и���Ȥ�¼���poller��delete��
                connectionCallback_(shared_from_this());

        }
        channel_->remove(); // ��channel��poller��ɾ����
}

void TcpConnection::shutdown()
{
        if (state_ == kConnected)
        {
                setState(kDisconnecting);
                loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
        }
}

// shutdown���û����õ� ���Ǵ�ʱд�������ܻ�û��ִ����� ��Ҫ�ж�
void TcpConnection::shutdownInLoop()
{
        if (!channel_->isWriting())// ˵����ǰoutputBuffer_�е������Ѿ�ȫ���������
        {
                // we are not writing
                socket_->shutdownWrite();//�ر�д��
        }

}