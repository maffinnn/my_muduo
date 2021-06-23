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
        // 注册回调函数
        server_.setConnectionCallback(std::bind(&EchoServer::onConnection, this, std::placeholders::_1));

        server_.setMessageCallback(std::bind(&EchoServer::onMessage, this, 
                std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        // 设置合适的loop线程数量
        server_.setThreadNum(3);
    }

    void start()
    {
        server_.start();
    }

    ~EchoServer();
private:
    // 连接建立或者断开的回调
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
    // 可读事件回调
    void onMessage(const TcpConnectionPtr& conn,
                Buffer* buf, 
                Timestamp time)
    {
        std::string msg = buf->retrieveAllAsString();
        conn->send(msg);
        // 服务器端主动关闭写端
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