#include "AsyncIOHandler.h"
#include "LogUtil.h"
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <errno.h>

/**
 * @brief AsyncIOHandler 构造函数
 * @details 创建epoll实例用于异步IO操作
 */
AsyncIOHandler::AsyncIOHandler() : epollFd(-1), running(false) {
    epollFd = epoll_create1(0);
    if (epollFd < 0) {
        LogUtil::error("Failed to create epoll instance");
    }
}

/**
 * @brief AsyncIOHandler 析构函数
 * @details 停止异步IO处理并关闭epoll文件描述符
 */
AsyncIOHandler::~AsyncIOHandler() {
    stop();
    if (epollFd >= 0) {
        close(epollFd);
    }
}

/**
 * @brief 启动异步IO处理器
 * @details 创建IO线程开始处理epoll事件
 */
void AsyncIOHandler::start() {
    if (!running && epollFd >= 0) {
        running = true;
        ioThreadObj = std::thread(&AsyncIOHandler::ioThread, this);
        LogUtil::info("AsyncIOHandler started");
    }
}

/**
 * @brief 停止异步IO处理器
 * @details 停止IO线程并等待其结束
 */
void AsyncIOHandler::stop() {
    if (running) {
        running = false;
        if (ioThreadObj.joinable()) {
            ioThreadObj.join();
        }
        LogUtil::info("AsyncIOHandler stopped");
    }
}

/**
 * @brief 注册文件描述符用于异步读取
 * @param fd 文件描述符
 * @param callback 读取完成后的回调函数
 */
void AsyncIOHandler::registerRead(int fd, std::function<void(int, const char*, size_t)> callback) {
    // 设置文件描述符为非阻塞模式
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    
    // 注册到epoll，使用ET模式（边缘触发）
    struct epoll_event event;
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = fd;
    
    if (epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &event) < 0) {
        LogUtil::error("Failed to register fd for read");
        return;
    }
    
    readCallbacks[fd] = callback;
}

/**
 * @brief 注册文件描述符用于异步写入
 * @param fd 文件描述符
 * @param data 要写入的数据
 * @param size 数据大小（字节）
 * @param callback 写入完成后的回调函数
 */
void AsyncIOHandler::registerWrite(int fd, const char* data, size_t size, std::function<void(int)> callback) {
    // 设置文件描述符为非阻塞模式
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    
    // 注册到epoll，使用ET模式（边缘触发）
    struct epoll_event event;
    event.events = EPOLLOUT | EPOLLET;
    event.data.fd = fd;
    
    if (epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &event) < 0) {
        LogUtil::error("Failed to register fd for write");
        return;
    }
    
    writeCallbacks[fd] = callback;
    writeBuffers[fd] = std::make_pair(data, size);
}

/**
 * @brief 移除文件描述符的监听
 * @param fd 文件描述符
 */
void AsyncIOHandler::remove(int fd) {
    epoll_ctl(epollFd, EPOLL_CTL_DEL, fd, nullptr);
    readCallbacks.erase(fd);
    writeCallbacks.erase(fd);
    writeBuffers.erase(fd);
}

/**
 * @brief 检查异步IO处理器是否正在运行
 * @return 正在运行返回true，否则返回false
 */
bool AsyncIOHandler::isRunning() const {
    return running;
}

/**
 * @brief IO线程主循环
 * @details 循环等待epoll事件并处理读写操作
 */
void AsyncIOHandler::ioThread() {
    const int MAX_EVENTS = 1024;
    struct epoll_event events[MAX_EVENTS];
    
    while (running) {
        // 等待epoll事件，超时1秒
        int nfds = epoll_wait(epollFd, events, MAX_EVENTS, 1000);
        if (nfds < 0) {
            LogUtil::error("epoll_wait failed");
            continue;
        }
        
        // 处理所有就绪事件
        for (int i = 0; i < nfds; i++) {
            int fd = events[i].data.fd;
            
            // 处理可读事件
            if (events[i].events & EPOLLIN) {
                char buffer[4096];
                ssize_t n;
                // ET模式需要循环读取所有可用数据
                while ((n = read(fd, buffer, sizeof(buffer))) > 0) {
                    auto it = readCallbacks.find(fd);
                    if (it != readCallbacks.end()) {
                        it->second(fd, buffer, n);
                    }
                }
                if (n == 0) {
                    // 连接关闭
                    remove(fd);
                }
                // n < 0 且 errno == EAGAIN 是正常的，说明数据已读完
            }
            
            // 处理可写事件
            if (events[i].events & EPOLLOUT) {
                auto bufferIt = writeBuffers.find(fd);
                auto callbackIt = writeCallbacks.find(fd);
                
                if (bufferIt != writeBuffers.end() && callbackIt != writeCallbacks.end()) {
                    const char* data = bufferIt->second.first;
                    size_t size = bufferIt->second.second;
                    
                    ssize_t n = write(fd, data, size);
                    if (n > 0) {
                        if (n == static_cast<ssize_t>(size)) {
                            // 写入完成
                            callbackIt->second(fd);
                            remove(fd);
                        } else {
                            // 更新缓冲区（处理部分写入）
                            bufferIt->second.first += n;
                            bufferIt->second.second -= n;
                        }
                    }
                }
            }
        }
    }
}
