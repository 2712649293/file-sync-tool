#include "LogUtil.h"
#include <ctime>
#include <iostream>

std::ofstream LogUtil::logFile;
LogLevel LogUtil::currentLevel = LOG_INFO;
bool LogUtil::consoleOutput = true;
std::mutex LogUtil::logMutex;

void LogUtil::init(const std::string& logPath, LogLevel level) {
    logFile.open(logPath, std::ios::app);
    currentLevel = level;
    consoleOutput = true;
}

void LogUtil::close() {
    if (logFile.is_open()) {
        logFile.close();
    }
}

std::string LogUtil::formatMessage(LogLevel level, const std::string& message) {
    time_t now = time(nullptr);
    struct tm* tm_info = localtime(&now);
    char time_buffer[64];
    strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", tm_info);
    
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
    
    return std::string(time_buffer) + " [" + level_str + "] " + message;
}

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

void LogUtil::setLogLevel(LogLevel level) {
    currentLevel = level;
}

void LogUtil::enableConsoleOutput(bool enable) {
    consoleOutput = enable;
}
