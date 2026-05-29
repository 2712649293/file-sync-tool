#include "TcpClient.h"
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include "LogUtil.h"
#include "ExceptionHandler.h"

/**
 * @brief TcpClient 构造函数
 * @details 初始化TCP客户端，设置默认超时时间为5秒
 */
TcpClient::TcpClient() : sockfd(-1), connected(false), timeoutMs(5000) {
}

/**
 * @brief TcpClient 析构函数
 * @details 断开连接并清理资源
 */
TcpClient::~TcpClient() {
    disconnect();
}

/**
 * @brief 连接到远程服务器
 * @param serverIP 服务器IP地址
 * @param port 服务器端口
 * @return 连接成功返回true，否则返回false
 */
bool TcpClient::connect(const std::string& serverIP, int port) {
    // 创建TCP socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        LogUtil::error("Failed to create socket");
        return false;
    }
    
    // 设置服务器地址结构
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    
    // 将IP地址转换为网络字节序
    if (inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr) <= 0) {
        LogUtil::error("Invalid server IP address");
        close(sockfd);
        sockfd = -1;
        return false;
    }
    
    // 建立连接
    if (::connect(sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        LogUtil::error("Failed to connect to server");
        close(sockfd);
        sockfd = -1;
        return false;
    }
    
    connected = true;
    LogUtil::info("Connected to server: " + serverIP + ":" + std::to_string(port));
    return true;
}

/**
 * @brief 断开与服务器的连接
 */
void TcpClient::disconnect() {
    if (sockfd >= 0) {
        close(sockfd);
        sockfd = -1;
    }
    connected = false;
    LogUtil::info("Disconnected from server");
}

/**
 * @brief 发送数据到服务器
 * @param data 数据指针
 * @param size 数据大小（字节）
 * @return 发送成功返回true，否则返回false
 */
bool TcpClient::send(const void* data, size_t size) {
    if (!connected) {
        LogUtil::error("Not connected to server");
        return false;
    }
    
    size_t totalSent = 0;
    // 循环发送直到所有数据都发送完毕
    while (totalSent < size) {
        ssize_t sent = ::send(sockfd, (char*)data + totalSent, size - totalSent, 0);
        if (sent < 0) {
            LogUtil::error("Failed to send data");
            connected = false;
            return false;
        }
        totalSent += sent;
    }
    
    return true;
}

/**
 * @brief 从服务器接收数据
 * @param buffer 接收缓冲区指针
 * @param size 缓冲区大小（字节）
 * @return 实际接收到的字节数
 */
size_t TcpClient::receive(void* buffer, size_t size) {
    if (!connected) {
        LogUtil::error("Not connected to server");
        return 0;
    }
    
    ssize_t received = ::recv(sockfd, buffer, size, 0);
    if (received < 0) {
        LogUtil::error("Failed to receive data");
        connected = false;
        return 0;
    } else if (received == 0) {
        LogUtil::info("Server closed connection");
        connected = false;
        return 0;
    }
    
    return received;
}

/**
 * @brief 检查是否已连接到服务器
 * @return 已连接返回true，否则返回false
 */
bool TcpClient::isConnected() const {
    return connected;
}

/**
 * @brief 获取底层socket文件描述符
 * @return socket文件描述符
 */
int TcpClient::getSocketFd() const {
    return sockfd;
}

/**
 * @brief 设置socket超时时间
 * @param timeout 超时时间（毫秒）
 */
void TcpClient::setTimeout(int timeout) {
    timeoutMs = timeout;
    struct timeval tv;
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

/**
 * @brief 发送心跳包到服务器
 * @return 发送成功返回true，否则返回false
 */
bool TcpClient::sendHeartbeat() {
    const char heartbeat[] = "HB";
    return send(heartbeat, sizeof(heartbeat));
}

/**
 * @brief 接收服务器的心跳包
 * @return 接收到有效的心跳包返回true，否则返回false
 */
bool TcpClient::receiveHeartbeat() {
    char buffer[3];
    size_t received = receive(buffer, sizeof(buffer));
    return received == sizeof("HB") && strcmp(buffer, "HB") == 0;
}
