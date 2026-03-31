#ifndef LOG_UTIL_H
#define LOG_UTIL_H

#include <string>
#include <fstream>
#include <mutex>

enum LogLevel {
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
};

class LogUtil {
public:
    // 初始化日志系统
    static void init(const std::string& logPath, LogLevel level = LOG_INFO);
    
    // 关闭日志系统
    static void close();
    
    // 日志输出方法
    static void info(const std::string& message);
    static void warn(const std::string& message);
    static void error(const std::string& message);
    
    // 设置日志级别
    static void setLogLevel(LogLevel level);
    
    // 启用控制台输出
    static void enableConsoleOutput(bool enable);
    
private:
    static std::ofstream logFile;
    static LogLevel currentLevel;
    static bool consoleOutput;
    static std::mutex logMutex;
    
    // 格式化日志消息
    static std::string formatMessage(LogLevel level, const std::string& message);
};

#endif // LOG_UTIL_H
