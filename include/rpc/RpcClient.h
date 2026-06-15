#ifndef SYNC_RPC_CLIENT_H
#define SYNC_RPC_CLIENT_H

#include <string>
#include <memory>
#include "sync_service.pb.h"

class RpcClient {
public:
    RpcClient();
    ~RpcClient();

    bool connect(const std::string& host, int port);

    void disconnect();

    bool listFiles(const std::string& directory, bool recursive,
                   synctool::rpc::ListFilesResponse& response);

    bool getFileMeta(const std::string& filePath,
                     synctool::rpc::GetFileMetaResponse& response);

    bool isConnected() const;

private:
    bool sendRequest(const std::string& request, synctool::rpc::RpcType rpcType,
                     synctool::rpc::RpcResponse& response);

    bool sendAll(const void* data, size_t size);
    bool recvAll(void* data, size_t size);

    int sockFd;
    bool connected;
};

#endif