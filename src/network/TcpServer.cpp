#include "TcpServer.h"
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include "LogUtil.h"

TcpServer::TcpServer() : serverSock(-1), running(false) {
}

TcpServer::~TcpServer() {
    stop();
}

bool TcpServer::start(int port) {
    serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock < 0) {
        LogUtil::error("Failed to create server socket");
        return false;
    }
    
    int opt = 1;
    if (setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        LogUtil::error("Failed to set socket options");
        close(serverSock);
        serverSock = -1;
        return false;
    }
    
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);
    
    if (bind(serverSock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        LogUtil::error("Failed to bind server socket");
        close(serverSock);
        serverSock = -1;
        return false;
    }
    
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

void TcpServer::stop() {
    if (serverSock >= 0) {
        close(serverSock);
        serverSock = -1;
    }
    running = false;
    LogUtil::info("Server stopped");
}

int TcpServer::accept() {
    if (!running) {
        LogUtil::error("Server is not running");
        return -1;
    }
    
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
    int clientSock = ::accept(serverSock, (struct sockaddr*)&clientAddr, &clientAddrLen);
    if (clientSock < 0) {
        LogUtil::error("Failed to accept client connection");
        return -1;
    }
    
    char clientIP[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(clientAddr.sin_addr), clientIP, INET_ADDRSTRLEN);
    LogUtil::info("Client connected: " + std::string(clientIP) + ":" + std::to_string(ntohs(clientAddr.sin_port)));
    
    return clientSock;
}

bool TcpServer::send(int clientSock, const void* data, size_t size) {
    size_t totalSent = 0;
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

void TcpServer::closeClient(int clientSock) {
    if (clientSock >= 0) {
        close(clientSock);
    }
    LogUtil::info("Client connection closed");
}

bool TcpServer::isRunning() const {
    return running;
}

bool TcpServer::sendHeartbeat(int clientSock) {
    const char heartbeat[] = "HB";
    return send(clientSock, heartbeat, sizeof(heartbeat));
}

bool TcpServer::receiveHeartbeat(int clientSock) {
    char buffer[3];
    size_t received = receive(clientSock, buffer, sizeof(buffer));
    return received == sizeof("HB") && strcmp(buffer, "HB") == 0;
}
