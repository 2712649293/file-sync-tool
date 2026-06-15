#ifndef SYNC_RPC_SERVER_H
#define SYNC_RPC_SERVER_H

#include <string>
#include "sync_service.pb.h"

class RpcServer {
public:
    RpcServer();
    ~RpcServer();

    void setBaseDirectory(const std::string& baseDir);

    bool handleRpcClient(int clientSock);

private:
    bool handleListFiles(const synctool::rpc::ListFilesRequest& request,
                         synctool::rpc::ListFilesResponse& response);

    bool handleGetFileMeta(const synctool::rpc::GetFileMetaRequest& request,
                           synctool::rpc::GetFileMetaResponse& response);

    bool receiveRequest(int clientSock, synctool::rpc::RpcRequest& request);
    bool sendResponse(int clientSock, const synctool::rpc::RpcResponse& response);

    bool sendAll(int sock, const void* data, size_t size);
    bool recvAll(int sock, void* data, size_t size);

    void listDirectory(const std::string& directory, bool recursive,
                       synctool::rpc::ListFilesResponse& response);

    std::string calculateFileMD5(const std::string& filePath);

    std::string baseDirectory;
};

#endif