#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <string>
#include <sys/socket.h>
#include <netinet/in.h>

class TcpServer {
public:
    TcpServer();
    ~TcpServer();
    
    // 启动服务器
    bool start(int port);
    
    // 停止服务器
    void stop();
    
    // 接受客户端连接
    int accept();
    
    // 发送数据
    bool send(int clientSock, const void* data, size_t size);
    
    // 接收数据
    size_t receive(int clientSock, void* buffer, size_t size);
    
    // 关闭客户端连接
    void closeClient(int clientSock);
    
    // 检查服务器状态
    bool isRunning() const;
    
    // 心跳检测
    bool sendHeartbeat(int clientSock);
    bool receiveHeartbeat(int clientSock);
    
private:
    int serverSock;
    struct sockaddr_in serverAddr;
    bool running;
};

#endif // TCP_SERVER_H
