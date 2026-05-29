#include "ExceptionHandler.h"
#include "LogUtil.h"

/**
 * @brief 处理同步异常
 * @param e 同步异常对象
 */
void ExceptionHandler::handleException(const SyncException& e) {
    LogUtil::error("Sync Exception: " + std::string(e.what()));
}

/**
 * @brief 处理网络异常
 * @param e 网络异常对象
 */
void ExceptionHandler::handleNetworkException(const NetworkException& e) {
    LogUtil::error("Network Exception: " + std::string(e.what()));
}

/**
 * @brief 处理文件异常
 * @param e 文件异常对象
 */
void ExceptionHandler::handleFileException(const FileException& e) {
    LogUtil::error("File Exception: " + std::string(e.what()));
}

/**
 * @brief 处理未知异常
 */
void ExceptionHandler::handleUnknownException() {
    LogUtil::error("Unknown Exception occurred");
}
