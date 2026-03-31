#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include <string>
#include <sys/socket.h>
#include <netinet/in.h>

class TcpClient {
public:
    TcpClient();
    ~TcpClient();
    
    // 连接到服务器
    bool connect(const std::string& serverIP, int port);
    
    // 断开连接
    void disconnect();
    
    // 发送数据
    bool send(const void* data, size_t size);
    
    // 接收数据
    size_t receive(void* buffer, size_t size);
    
    // 检查连接状态
    bool isConnected() const;
    
    // 设置超时时间（毫秒）
    void setTimeout(int timeout);
    
    // 心跳检测
    bool sendHeartbeat();
    bool receiveHeartbeat();
    
private:
    int sockfd;
    struct sockaddr_in serverAddr;
    bool connected;
    int timeoutMs;
};

#endif // TCP_CLIENT_H
