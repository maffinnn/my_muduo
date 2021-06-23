#include "Acceptor.h"
#include "Logger.h"
#include "InetAddress.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <fcntl.h>

// �����ȫ��static ԭ��Ϊ�˷�ֹ�������ļ����������� 
static int createNonblocking()
{                 //��ʾ�� IPv4  �� tcp�׽��֣� ������IO�� �ӽ��̼̳и�����ʱ�رն�Ӧ��fd
    int sockfd = ::socket(AF_INET, SOCK_STREAM |SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    if (sockfd<0)
    {
        LOG_FATAL("%s:%s:%d listen socket create err: %d \n",__FILE__, __FUNCTION__, __LINE__, errno);
    }
    return sockfd;
}
Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport)
    : loop_(loop)
    , acceptSocket_(createNonblocking())
    , acceptChannel_(loop, acceptSocket_.get_fd())
    , listenning_(false)
    , idleFd_(::open("/dev/null", O_RDONLY|SOCK_CLOEXEC))
{
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(true);
    acceptSocket_.bindAddress(listenAddr);
    // TcpServer::start() ���� Acceptor::listen()
    // ��������û������� Ҫִ��һ���ص� ��connfd�����channel�ٸ���subloop
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));


} 
// ��listenfd�յ����¼�ʱ�����
void Acceptor::handleRead()
{
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(&peerAddr);
    if (connfd >= 0)
    {
        if (newConnectionCallback_)
        {
            // ��ѯ�ҵ�subloop ���ѷַ���ǰ���¿ͻ��˵�Channel
            newConnectionCallback_(connfd, peerAddr);
        }
        else
        {
            ::close(connfd);
        }
    }
    else
    {
        LOG_ERROR("%s:%s:%d accept err: %d \n",__FILE__, __FUNCTION__, __LINE__, errno);
        if (errno==EMFILE)// reach maxium file descriptor number
        {
            ::close(idleFd_);
            idleFd_ = :: accept(acceptSocket_.get_fd(), NULL, NULL);
            ::close(idleFd_);
            idleFd_ = ::open("/dev/null", O_RDONLY|SOCK_CLOEXEC);
            LOG_ERROR("%s:%s:%d Resource reach maximun limit \n",__FILE__, __FUNCTION__, __LINE__);
        }
    }
}  
Acceptor::~Acceptor()
{
    acceptChannel_.disableAll();
    acceptChannel_.remove();// ���Լ�����ɾ��
    ::close(idleFd_);
}

void Acceptor::listen()
{
    listenning_ = true;
    acceptSocket_.listen();
    acceptChannel_.enableReading();

}

