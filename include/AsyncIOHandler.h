#ifndef ASYNC_IO_HANDLER_H
#define ASYNC_IO_HANDLER_H

#include <sys/epoll.h>
#include <vector>
#include <unordered_map>
#include <functional>
#include <thread>

class AsyncIOHandler {
public:
    // 构造函数
    AsyncIOHandler();
    
    // 析构函数
    ~AsyncIOHandler();
    
    // 启动异步IO处理线程
    void start();
    
    // 停止异步IO处理线程
    void stop();
    
    // 注册文件描述符进行异步读取
    void registerRead(int fd, std::function<void(int, const char*, size_t)> callback);
    
    // 注册文件描述符进行异步写入
    void registerWrite(int fd, const char* data, size_t size, std::function<void(int)> callback);
    
    // 移除文件描述符
    void remove(int fd);
    
    // 检查是否运行
    bool isRunning() const;
    
private:
    // 异步IO处理线程函数
    void ioThread();
    
    // epoll文件描述符
    int epollFd;
    
    // 处理线程
    std::thread ioThreadObj;
    
    // 运行标志
    bool running;
    
    // 回调函数映射
    std::unordered_map<int, std::function<void(int, const char*, size_t)>> readCallbacks;
    std::unordered_map<int, std::function<void(int)>> writeCallbacks;
    
    // 写入数据缓存
    std::unordered_map<int, std::pair<const char*, size_t>> writeBuffers;
};

#endif // ASYNC_IO_HANDLER_H
