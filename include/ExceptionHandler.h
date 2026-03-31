#ifndef EXCEPTION_HANDLER_H
#define EXCEPTION_HANDLER_H

#include <string>
#include <stdexcept>

class SyncException : public std::runtime_error {
public:
    SyncException(const std::string& message) : std::runtime_error(message) {}
};

class NetworkException : public SyncException {
public:
    NetworkException(const std::string& message) : SyncException(message) {}
};

class FileException : public SyncException {
public:
    FileException(const std::string& message) : SyncException(message) {}
};

class ExceptionHandler {
public:
    // 处理异常并记录日志
    static void handleException(const SyncException& e);
    
    // 处理网络异常
    static void handleNetworkException(const NetworkException& e);
    
    // 处理文件异常
    static void handleFileException(const FileException& e);
    
    // 处理未知异常
    static void handleUnknownException();
};

#endif // EXCEPTION_HANDLER_H
