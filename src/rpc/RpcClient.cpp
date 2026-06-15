#include "rpc/RpcClient.h"
#include "LogUtil.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

RpcClient::RpcClient() : sockFd(-1), connected(false) {}

RpcClient::~RpcClient() {
    disconnect();
}

bool RpcClient::connect(const std::string& host, int port) {
    sockFd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockFd < 0) {
        LogUtil::error("RpcClient: failed to create socket");
        return false;
    }

    struct sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    if (inet_pton(AF_INET, host.c_str(), &serverAddr.sin_addr) <= 0) {
        LogUtil::error("RpcClient: invalid address: " + host);
        close(sockFd);
        sockFd = -1;
        return false;
    }

    if (::connect(sockFd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        LogUtil::error("RpcClient: connection failed to " + host + ":" + std::to_string(port));
        close(sockFd);
        sockFd = -1;
        return false;
    }

    // 发送模式标识：1 = RPC 模式
    uint8_t mode = 1;
    if (!sendAll(&mode, sizeof(mode))) {
        LogUtil::error("RpcClient: failed to send mode byte");
        disconnect();
        return false;
    }

    connected = true;
    LogUtil::info("RpcClient: connected to " + host + ":" + std::to_string(port));
    return true;
}

void RpcClient::disconnect() {
    if (sockFd >= 0) {
        close(sockFd);
        sockFd = -1;
    }
    connected = false;
}

bool RpcClient::isConnected() const {
    return connected;
}

bool RpcClient::listFiles(const std::string& directory, bool recursive,
                          synctool::rpc::ListFilesResponse& response) {
    synctool::rpc::RpcRequest rpcRequest;
    rpcRequest.set_type(synctool::rpc::RPC_LIST_FILES);
    synctool::rpc::ListFilesRequest* req = rpcRequest.mutable_list_files();
    req->set_directory(directory);
    req->set_recursive(recursive);

    std::string serialized;
    if (!rpcRequest.SerializeToString(&serialized)) {
        LogUtil::error("RpcClient: failed to serialize ListFilesRequest");
        return false;
    }

    synctool::rpc::RpcResponse rpcResponse;
    if (!sendRequest(serialized, synctool::rpc::RPC_LIST_FILES, rpcResponse)) {
        return false;
    }

    if (rpcResponse.success() && rpcResponse.has_list_files()) {
        response = rpcResponse.list_files();
        return true;
    }

    LogUtil::error("RpcClient: listFiles failed: " + rpcResponse.error());
    return false;
}

bool RpcClient::getFileMeta(const std::string& filePath,
                            synctool::rpc::GetFileMetaResponse& response) {
    synctool::rpc::RpcRequest rpcRequest;
    rpcRequest.set_type(synctool::rpc::RPC_GET_FILE_META);
    synctool::rpc::GetFileMetaRequest* req = rpcRequest.mutable_get_file_meta();
    req->set_file_path(filePath);

    std::string serialized;
    if (!rpcRequest.SerializeToString(&serialized)) {
        LogUtil::error("RpcClient: failed to serialize GetFileMetaRequest");
        return false;
    }

    synctool::rpc::RpcResponse rpcResponse;
    if (!sendRequest(serialized, synctool::rpc::RPC_GET_FILE_META, rpcResponse)) {
        return false;
    }

    if (rpcResponse.success() && rpcResponse.has_get_file_meta()) {
        response = rpcResponse.get_file_meta();
        return true;
    }

    LogUtil::error("RpcClient: getFileMeta failed: " + rpcResponse.error());
    return false;
}

bool RpcClient::sendRequest(const std::string& request, synctool::rpc::RpcType rpcType,
                            synctool::rpc::RpcResponse& response) {
    if (!connected) {
        LogUtil::error("RpcClient: not connected");
        return false;
    }

    // 发送：rpc_type(4) + payload_len(4) + payload
    uint32_t type = static_cast<uint32_t>(rpcType);
    uint32_t payloadLen = static_cast<uint32_t>(request.size());

    if (!sendAll(&type, sizeof(type))) return false;
    if (!sendAll(&payloadLen, sizeof(payloadLen))) return false;
    if (!sendAll(request.data(), payloadLen)) return false;

    // 接收：rpc_type(4) + payload_len(4) + payload
    uint32_t respType = 0;
    uint32_t respLen = 0;

    if (!recvAll(&respType, sizeof(respType))) return false;
    if (!recvAll(&respLen, sizeof(respLen))) return false;

    if (respLen == 0) {
        LogUtil::error("RpcClient: empty response");
        return false;
    }

    std::string respData(respLen, '\0');
    if (!recvAll(&respData[0], respLen)) return false;

    if (!response.ParseFromString(respData)) {
        LogUtil::error("RpcClient: failed to parse response");
        return false;
    }

    return true;
}

bool RpcClient::sendAll(const void* data, size_t size) {
    const char* ptr = static_cast<const char*>(data);
    size_t sent = 0;
    while (sent < size) {
        ssize_t n = send(sockFd, ptr + sent, size - sent, 0);
        if (n <= 0) {
            LogUtil::error("RpcClient: send failed");
            return false;
        }
        sent += n;
    }
    return true;
}

bool RpcClient::recvAll(void* data, size_t size) {
    char* ptr = static_cast<char*>(data);
    size_t received = 0;
    while (received < size) {
        ssize_t n = recv(sockFd, ptr + received, size - received, 0);
        if (n <= 0) {
            LogUtil::error("RpcClient: recv failed");
            return false;
        }
        received += n;
    }
    return true;
}