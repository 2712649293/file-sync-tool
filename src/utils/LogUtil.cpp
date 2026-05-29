#include "LogUtil.h"
#include <ctime>
#include <iostream>

// 静态成员变量初始化
std::ofstream LogUtil::logFile;
LogLevel LogUtil::currentLevel = LOG_INFO;
bool LogUtil::consoleOutput = true;
std::mutex LogUtil::logMutex;

/**
 * @brief 初始化日志工具
 * @param logPath 日志文件路径
 * @param level 日志级别（默认LOG_INFO）
 */
void LogUtil::init(const std::string& logPath, LogLevel level) {
    logFile.open(logPath, std::ios::app);
    currentLevel = level;
    consoleOutput = true;
}

/**
 * @brief 关闭日志工具
 * @details 关闭日志文件流
 */
void LogUtil::close() {
    if (logFile.is_open()) {
        logFile.close();
    }
}

/**
 * @brief 格式化日志消息
 * @param level 日志级别
 * @param message 日志消息内容
 * @return 格式化后的日志字符串
 */
std::string LogUtil::formatMessage(LogLevel level, const std::string& message) {
    // 获取当前时间
    time_t now = time(nullptr);
    struct tm* tm_info = localtime(&now);
    char time_buffer[64];
    strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", tm_info);
    
    // 根据级别获取级别字符串
    std::string level_str;
    switch (level) {
        case LOG_INFO:
            level_str = "INFO";
            break;
        case LOG_WARN:
            level_str = "WARN";
            break;
        case LOG_ERROR:
            level_str = "ERROR";
            break;
    }
    
    // 格式化输出：时间 [级别] 消息
    return std::string(time_buffer) + " [" + level_str + "] " + message;
}

/**
 * @brief 输出INFO级别日志
 * @param message 日志消息内容
 */
void LogUtil::info(const std::string& message) {
    if (currentLevel <= LOG_INFO) {
        std::lock_guard<std::mutex> lock(logMutex);
        std::string formatted = formatMessage(LOG_INFO, message);
        if (consoleOutput) {
            std::cout << formatted << std::endl;
        }
        if (logFile.is_open()) {
            logFile << formatted << std::endl;
        }
    }
}

/**
 * @brief 输出WARN级别日志
 * @param message 日志消息内容
 */
void LogUtil::warn(const std::string& message) {
    if (currentLevel <= LOG_WARN) {
        std::lock_guard<std::mutex> lock(logMutex);
        std::string formatted = formatMessage(LOG_WARN, message);
        if (consoleOutput) {
            std::cout << formatted << std::endl;
        }
        if (logFile.is_open()) {
            logFile << formatted << std::endl;
        }
    }
}

/**
 * @brief 输出ERROR级别日志
 * @param message 日志消息内容
 */
void LogUtil::error(const std::string& message) {
    if (currentLevel <= LOG_ERROR) {
        std::lock_guard<std::mutex> lock(logMutex);
        std::string formatted = formatMessage(LOG_ERROR, message);
        if (consoleOutput) {
            std::cerr << formatted << std::endl;
        }
        if (logFile.is_open()) {
            logFile << formatted << std::endl;
        }
    }
}

/**
 * @brief 设置日志级别
 * @param level 日志级别
 */
void LogUtil::setLogLevel(LogLevel level) {
    currentLevel = level;
}

/**
 * @brief 设置是否输出到控制台
 * @param enable true表示启用控制台输出，false表示禁用
 */
void LogUtil::enableConsoleOutput(bool enable) {
    consoleOutput = enable;
}
