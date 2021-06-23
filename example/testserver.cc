#include <my_muduo/TcpServer.h>
#include <my_muduo/Logger.h>

#include <string>
#include <fucntional>

class EchoServer
{
public:
    EchoServer(EventLoop* loop,
        const InetAddress& addr,
        const std::string& name,)
        : server_(loop, addr, name)
        , loop_(loop)
    {
        // ע��ص�����
        server_.setConnectionCallback(std::bind(&EchoServer::onConnection, this, std::placeholders::_1));

        server_.setMessageCallback(std::bind(&EchoServer::onMessage, this, 
                std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        // ���ú��ʵ�loop�߳�����
        server_.setThreadNum(3);
    }

    void start()
    {
        server_.start();
    }

    ~EchoServer();
private:
    // ���ӽ������߶Ͽ��Ļص�
    void onConnection(const TcpConnectionPtr& conn)
    {
        if (conn->connected)
        {
            LOG_INFO("Connection UP : %s\n" conn->peerAddress().toIpPort().c_str());
        }
        else
        {
            LOG_INFO("Connection DOWN : %s\n" conn->peerAddress().toIpPort().c_str());
        }
    }
    // �ɶ��¼��ص�
    void onMessage(const TcpConnectionPtr& conn,
                Buffer* buf, 
                Timestamp time)
    {
        std::string msg = buf->retrieveAllAsString();
        conn->send(msg);
        // �������������ر�д��
        conn->shutdown();
    }

    
    EventLoop* loop_;
    TcpServer server_;
};

int main()
{
    EventLoop loop;
    InetAddress addr(8000);
    EchoServer echoserver(&loop, addr, "EchoServer0.0");
    echoserver.start();
    
    return 0;
}