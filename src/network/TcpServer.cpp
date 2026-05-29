#include "TcpServer.h"
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include "LogUtil.h"

/**
 * @brief TcpServer 构造函数
 * @details 初始化TCP服务器，设置serverSock为-1，表示未启动
 */
TcpServer::TcpServer() : serverSock(-1), running(false) {
}

/**
 * @brief TcpServer 析构函数
 * @details 停止服务器并清理资源
 */
TcpServer::~TcpServer() {
    stop();
}

/**
 * @brief 启动TCP服务器
 * @param port 监听端口
 * @return 启动成功返回true，否则返回false
 */
bool TcpServer::start(int port) {
    // 创建TCP socket
    serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock < 0) {
        LogUtil::error("Failed to create server socket");
        return false;
    }
    
    // 设置socket选项，允许端口复用
    int opt = 1;
    if (setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        LogUtil::error("Failed to set socket options");
        close(serverSock);
        serverSock = -1;
        return false;
    }
    
    // 设置服务器地址结构
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;  // 监听所有接口
    serverAddr.sin_port = htons(port);
    
    // 绑定socket到指定端口
    if (bind(serverSock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        LogUtil::error("Failed to bind server socket");
        close(serverSock);
        serverSock = -1;
        return false;
    }
    
    // 开始监听，最大等待队列长度为5
    if (listen(serverSock, 5) < 0) {
        LogUtil::error("Failed to listen on server socket");
        close(serverSock);
        serverSock = -1;
        return false;
    }
    
    running = true;
    LogUtil::info("Server started on port: " + std::to_string(port));
    return true;
}

/**
 * @brief 停止TCP服务器
 */
void TcpServer::stop() {
    if (serverSock >= 0) {
        close(serverSock);
        serverSock = -1;
    }
    running = false;
    LogUtil::info("Server stopped");
}

/**
 * @brief 接受客户端连接
 * @return 客户端socket文件描述符，失败返回-1
 */
int TcpServer::accept() {
    if (!running) {
        LogUtil::error("Server is not running");
        return -1;
    }
    
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
    
    // 接受客户端连接
    int clientSock = ::accept(serverSock, (struct sockaddr*)&clientAddr, &clientAddrLen);
    if (clientSock < 0) {
        LogUtil::error("Failed to accept client connection");
        return -1;
    }
    
    // 获取客户端IP地址
    char clientIP[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(clientAddr.sin_addr), clientIP, INET_ADDRSTRLEN);
    LogUtil::info("Client connected: " + std::string(clientIP) + ":" + std::to_string(ntohs(clientAddr.sin_port)));
    
    return clientSock;
}

/**
 * @brief 发送数据给客户端
 * @param clientSock 客户端socket文件描述符
 * @param data 数据指针
 * @param size 数据大小（字节）
 * @return 发送成功返回true，否则返回false
 */
bool TcpServer::send(int clientSock, const void* data, size_t size) {
    size_t totalSent = 0;
    // 循环发送直到所有数据都发送完毕
    while (totalSent < size) {
        ssize_t sent = ::send(clientSock, (char*)data + totalSent, size - totalSent, 0);
        if (sent < 0) {
            LogUtil::error("Failed to send data to client");
            return false;
        }
        totalSent += sent;
    }
    
    return true;
}

/**
 * @brief 从客户端接收数据
 * @param clientSock 客户端socket文件描述符
 * @param buffer 接收缓冲区指针
 * @param size 缓冲区大小（字节）
 * @return 实际接收到的字节数
 */
size_t TcpServer::receive(int clientSock, void* buffer, size_t size) {
    ssize_t received = ::recv(clientSock, buffer, size, 0);
    if (received < 0) {
        LogUtil::error("Failed to receive data from client");
        return 0;
    } else if (received == 0) {
        LogUtil::info("Client closed connection");
        return 0;
    }
    
    return received;
}

/**
 * @brief 关闭客户端连接
 * @param clientSock 客户端socket文件描述符
 */
void TcpServer::closeClient(int clientSock) {
    if (clientSock >= 0) {
        close(clientSock);
    }
    LogUtil::info("Client connection closed");
}

/**
 * @brief 检查服务器是否正在运行
 * @return 正在运行返回true，否则返回false
 */
bool TcpServer::isRunning() const {
    return running;
}

/**
 * @brief 获取服务器socket文件描述符
 * @return 服务器socket文件描述符
 */
int TcpServer::getServerSocket() const {
    return serverSock;
}

/**
 * @brief 发送心跳包给客户端
 * @param clientSock 客户端socket文件描述符
 * @return 发送成功返回true，否则返回false
 */
bool TcpServer::sendHeartbeat(int clientSock) {
    const char heartbeat[] = "HB";
    return send(clientSock, heartbeat, sizeof(heartbeat));
}

/**
 * @brief 接收客户端的心跳包
 * @param clientSock 客户端socket文件描述符
 * @return 接收到有效的心跳包返回true，否则返回false
 */
bool TcpServer::receiveHeartbeat(int clientSock) {
    char buffer[3];
    size_t received = receive(clientSock, buffer, sizeof(buffer));
    return received == sizeof("HB") && strcmp(buffer, "HB") == 0;
}
