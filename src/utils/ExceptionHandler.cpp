#include "ExceptionHandler.h"
#include "LogUtil.h"

void ExceptionHandler::handleException(const SyncException& e) {
    LogUtil::error("Sync Exception: " + std::string(e.what()));
}

void ExceptionHandler::handleNetworkException(const NetworkException& e) {
    LogUtil::error("Network Exception: " + std::string(e.what()));
}

void ExceptionHandler::handleFileException(const FileException& e) {
    LogUtil::error("File Exception: " + std::string(e.what()));
}

void ExceptionHandler::handleUnknownException() {
    LogUtil::error("Unknown Exception occurred");
}
