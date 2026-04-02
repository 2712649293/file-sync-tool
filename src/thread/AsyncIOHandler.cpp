#include "AsyncIOHandler.h"
#include "LogUtil.h"
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <errno.h>

AsyncIOHandler::AsyncIOHandler() : epollFd(-1), running(false) {
    epollFd = epoll_create1(0);
    if (epollFd < 0) {
        LogUtil::error("Failed to create epoll instance");
    }
}

AsyncIOHandler::~AsyncIOHandler() {
    stop();
    if (epollFd >= 0) {
        close(epollFd);
    }
}

void AsyncIOHandler::start() {
    if (!running && epollFd >= 0) {
        running = true;
        ioThreadObj = std::thread(&AsyncIOHandler::ioThread, this);
        LogUtil::info("AsyncIOHandler started");
    }
}

void AsyncIOHandler::stop() {
    if (running) {
        running = false;
        if (ioThreadObj.joinable()) {
            ioThreadObj.join();
        }
        LogUtil::info("AsyncIOHandler stopped");
    }
}

void AsyncIOHandler::registerRead(int fd, std::function<void(int, const char*, size_t)> callback) {
    // 设置文件描述符为非阻塞
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    
    // 注册到epoll
    struct epoll_event event;
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = fd;
    
    if (epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &event) < 0) {
        LogUtil::error("Failed to register fd for read");
        return;
    }
    
    readCallbacks[fd] = callback;
}

void AsyncIOHandler::registerWrite(int fd, const char* data, size_t size, std::function<void(int)> callback) {
    // 设置文件描述符为非阻塞
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    
    // 注册到epoll
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

void AsyncIOHandler::remove(int fd) {
    epoll_ctl(epollFd, EPOLL_CTL_DEL, fd, nullptr);
    readCallbacks.erase(fd);
    writeCallbacks.erase(fd);
    writeBuffers.erase(fd);
}

bool AsyncIOHandler::isRunning() const {
    return running;
}

void AsyncIOHandler::ioThread() {
    const int MAX_EVENTS = 1024;
    struct epoll_event events[MAX_EVENTS];
    
    while (running) {
        int nfds = epoll_wait(epollFd, events, MAX_EVENTS, 1000);
        if (nfds < 0) {
            LogUtil::error("epoll_wait failed");
            continue;
        }
        
        for (int i = 0; i < nfds; i++) {
            int fd = events[i].data.fd;
            
            if (events[i].events & EPOLLIN) {
                // 处理可读事件
                char buffer[4096];
                ssize_t n;
                // 循环读取所有可用数据（ET模式需要这样做）
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
            
            if (events[i].events & EPOLLOUT) {
                // 处理可写事件
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
                            // 更新缓冲区
                            bufferIt->second.first += n;
                            bufferIt->second.second -= n;
                        }
                    }
                }
            }
        }
    }
}
