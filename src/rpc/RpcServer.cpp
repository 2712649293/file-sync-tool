#include "rpc/RpcServer.h"
#include "LogUtil.h"
#include "LinuxFileUtil.h"

#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <openssl/md5.h>
#include <fstream>
#include <iomanip>
#include <sstream>

RpcServer::RpcServer() {}

RpcServer::~RpcServer() {}

void RpcServer::setBaseDirectory(const std::string& baseDir) {
    baseDirectory = baseDir;
    if (!baseDirectory.empty() && baseDirectory.back() != '/') {
        baseDirectory += '/';
    }
}

bool RpcServer::handleRpcClient(int clientSock) {
    LogUtil::info("RpcServer: handling RPC client, fd=" + std::to_string(clientSock));

    bool firstRequest = true;
    while (true) {
        synctool::rpc::RpcRequest request;
        if (!receiveRequest(clientSock, request)) {
            if (firstRequest) {
                LogUtil::error("RpcServer: failed to receive request");
            } else {
                LogUtil::info("RpcServer: client disconnected, fd=" + std::to_string(clientSock));
            }
            close(clientSock);
            return firstRequest;
        }
        firstRequest = false;

        synctool::rpc::RpcResponse response;
        response.set_type(request.type());

        bool handled = false;
        switch (request.type()) {
            case synctool::rpc::RPC_LIST_FILES: {
                synctool::rpc::ListFilesResponse resp;
                handled = handleListFiles(request.list_files(), resp);
                if (handled) {
                    *response.mutable_list_files() = resp;
                }
                break;
            }
            case synctool::rpc::RPC_GET_FILE_META: {
                synctool::rpc::GetFileMetaResponse resp;
                handled = handleGetFileMeta(request.get_file_meta(), resp);
                if (handled) {
                    *response.mutable_get_file_meta() = resp;
                }
                break;
            }
            default:
                response.set_success(false);
                response.set_error("unknown RPC type");
                handled = false;
                break;
        }

        response.set_success(handled);
        if (!handled && response.error().empty()) {
            response.set_error("processing failed");
        }

        if (!sendResponse(clientSock, response)) {
            LogUtil::error("RpcServer: failed to send response");
            close(clientSock);
            return false;
        }
    }
}

bool RpcServer::handleListFiles(const synctool::rpc::ListFilesRequest& request,
                                synctool::rpc::ListFilesResponse& response) {
    std::string fullPath = baseDirectory + request.directory();
    struct stat st;
    if (stat(fullPath.c_str(), &st) != 0) {
        LogUtil::error("RpcServer: directory not found: " + fullPath);
        return false;
    }

    if (!S_ISDIR(st.st_mode)) {
        LogUtil::error("RpcServer: not a directory: " + fullPath);
        return false;
    }

    response.set_directory(request.directory());
    listDirectory(request.directory(), request.recursive(), response);

    LogUtil::info("RpcServer: listed " + std::to_string(response.files().size()) +
                 " entries in " + request.directory());
    return true;
}

bool RpcServer::handleGetFileMeta(const synctool::rpc::GetFileMetaRequest& request,
                                  synctool::rpc::GetFileMetaResponse& response) {
    std::string fullPath = baseDirectory + request.file_path();

    struct stat st;
    if (stat(fullPath.c_str(), &st) != 0) {
        response.mutable_meta()->set_exists(false);
        response.set_success(true);
        return true;
    }

    synctool::rpc::FileMeta* meta = response.mutable_meta();
    meta->set_exists(true);
    meta->set_is_directory(S_ISDIR(st.st_mode));
    meta->set_size(static_cast<int64_t>(st.st_size));
    meta->set_mtime(static_cast<int64_t>(st.st_mtime));
    meta->set_ctime(static_cast<int64_t>(st.st_ctime));
    meta->set_mode(static_cast<uint32_t>(st.st_mode));

    size_t slashPos = request.file_path().find_last_of('/');
    if (slashPos != std::string::npos) {
        meta->set_name(request.file_path().substr(slashPos + 1));
    } else {
        meta->set_name(request.file_path());
    }
    meta->set_path(request.file_path());

    if (!meta->is_directory()) {
        meta->set_md5(calculateFileMD5(fullPath));
    }

    response.set_success(true);
    LogUtil::info("RpcServer: get meta for " + request.file_path() +
                 (meta->is_directory() ? " [dir]" : " [file]"));
    return true;
}

bool RpcServer::receiveRequest(int clientSock, synctool::rpc::RpcRequest& request) {
    uint32_t type;
    uint32_t payloadLen;

    if (!recvAll(clientSock, &type, sizeof(type))) return false;
    if (!recvAll(clientSock, &payloadLen, sizeof(payloadLen))) return false;

    if (payloadLen == 0) {
        LogUtil::error("RpcServer: empty payload");
        return false;
    }

    std::string payload(payloadLen, '\0');
    if (!recvAll(clientSock, &payload[0], payloadLen)) return false;

    if (!request.ParseFromString(payload)) {
        LogUtil::error("RpcServer: failed to parse request");
        return false;
    }

    return true;
}

bool RpcServer::sendResponse(int clientSock, const synctool::rpc::RpcResponse& response) {
    std::string serialized;
    if (!response.SerializeToString(&serialized)) {
        LogUtil::error("RpcServer: failed to serialize response");
        return false;
    }

    uint32_t type = static_cast<uint32_t>(response.type());
    uint32_t payloadLen = static_cast<uint32_t>(serialized.size());

    if (!sendAll(clientSock, &type, sizeof(type))) return false;
    if (!sendAll(clientSock, &payloadLen, sizeof(payloadLen))) return false;
    if (!sendAll(clientSock, serialized.data(), payloadLen)) return false;

    return true;
}

bool RpcServer::sendAll(int sock, const void* data, size_t size) {
    const char* ptr = static_cast<const char*>(data);
    size_t sent = 0;
    while (sent < size) {
        ssize_t n = send(sock, ptr + sent, size - sent, 0);
        if (n <= 0) {
            if (n < 0 && errno == EINTR) continue;
            LogUtil::error("RpcServer: send failed");
            return false;
        }
        sent += n;
    }
    return true;
}

bool RpcServer::recvAll(int sock, void* data, size_t size) {
    char* ptr = static_cast<char*>(data);
    size_t received = 0;
    while (received < size) {
        ssize_t n = recv(sock, ptr + received, size - received, 0);
        if (n <= 0) {
            if (n < 0 && errno == EINTR) continue;
            LogUtil::error("RpcServer: recv failed");
            return false;
        }
        received += n;
    }
    return true;
}

void RpcServer::listDirectory(const std::string& relDir, bool recursive,
                              synctool::rpc::ListFilesResponse& response) {
    std::string fullRel = relDir;
    if (!fullRel.empty() && fullRel.back() != '/') {
        fullRel += '/';
    }

    std::string fullPath = baseDirectory + fullRel;
    DIR* dir = opendir(fullPath.c_str());
    if (!dir) {
        LogUtil::warn("RpcServer: cannot open directory: " + fullPath);
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;

        std::string entryRel = fullRel + name;
        std::string entryFullPath = baseDirectory + entryRel;

        struct stat st;
        if (stat(entryFullPath.c_str(), &st) != 0) {
            continue;
        }

        synctool::rpc::FileEntry* fileEntry = response.add_files();
        fileEntry->set_name(name);
        fileEntry->set_path(entryRel);
        fileEntry->set_is_directory(S_ISDIR(st.st_mode));
        fileEntry->set_size(static_cast<int64_t>(st.st_size));
        fileEntry->set_mtime(static_cast<int64_t>(st.st_mtime));
        fileEntry->set_mode(static_cast<uint32_t>(st.st_mode));

        response.set_total_count(response.total_count() + 1);

        if (recursive && S_ISDIR(st.st_mode)) {
            listDirectory(entryRel, recursive, response);
        }
    }

    closedir(dir);
}

std::string RpcServer::calculateFileMD5(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        return "";
    }

    MD5_CTX md5Ctx;
    MD5_Init(&md5Ctx);

    const size_t bufSize = 4096;
    char buf[bufSize];

    while (file.read(buf, bufSize)) {
        MD5_Update(&md5Ctx, buf, file.gcount());
    }
    if (file.gcount() > 0) {
        MD5_Update(&md5Ctx, buf, file.gcount());
    }

    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5_Final(digest, &md5Ctx);

    std::ostringstream oss;
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
    }
    return oss.str();
}