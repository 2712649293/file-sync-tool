#include "TcpClient.h"
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include "LogUtil.h"
#include "ExceptionHandler.h"

TcpClient::TcpClient() : sockfd(-1), connected(false), timeoutMs(5000) {
}

TcpClient::~TcpClient() {
    disconnect();
}

bool TcpClient::connect(const std::string& serverIP, int port) {
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        LogUtil::error("Failed to create socket");
        return false;
    }
    
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr) <= 0) {
        LogUtil::error("Invalid server IP address");
        close(sockfd);
        sockfd = -1;
        return false;
    }
    
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

void TcpClient::disconnect() {
    if (sockfd >= 0) {
        close(sockfd);
        sockfd = -1;
    }
    connected = false;
    LogUtil::info("Disconnected from server");
}

bool TcpClient::send(const void* data, size_t size) {
    if (!connected) {
        LogUtil::error("Not connected to server");
        return false;
    }
    
    size_t totalSent = 0;
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

bool TcpClient::isConnected() const {
    return connected;
}

void TcpClient::setTimeout(int timeout) {
    timeoutMs = timeout;
    struct timeval tv;
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

bool TcpClient::sendHeartbeat() {
    const char heartbeat[] = "HB";
    return send(heartbeat, sizeof(heartbeat));
}

bool TcpClient::receiveHeartbeat() {
    char buffer[3];
    size_t received = receive(buffer, sizeof(buffer));
    return received == sizeof("HB") && strcmp(buffer, "HB") == 0;
}
