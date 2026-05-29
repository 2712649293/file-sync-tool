#include "SyncServer.h"
#include "TcpServer.h"
#include "ThreadPool.h"
#include "AsyncIOHandler.h"
#include "LinuxFileUtil.h"
#include "LogUtil.h"

#include <cstring>
#include <cerrno>
#include <thread>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/epoll.h>

/**
 * @brief SyncServer 构造函数
 * @details 初始化同步服务器，设置默认状态
 */
SyncServer::SyncServer()
    : tcpServer(nullptr)
    , threadPool(nullptr)
    , asyncIOHandler(nullptr)
    , running(false)
    , processedTasks(0) {
}

/**
 * @brief SyncServer 析构函数
 * @details 停止服务器并释放所有资源
 */
SyncServer::~SyncServer() {
    stop();
    delete tcpServer;
    delete threadPool;
    delete asyncIOHandler;
}

/**
 * @brief 启动同步服务器
 * @param port 监听端口
 * @param threadCount 工作线程数（默认8）
 * @return 启动成功返回true，否则返回false
 */
bool SyncServer::start(int port, size_t threadCount) {
    if (running.load()) {
        LogUtil::error("SyncServer is already running");
        return false;
    }

    // 创建TCP服务器并启动
    tcpServer = new TcpServer();
    if (!tcpServer->start(port)) {
        LogUtil::error("Failed to start TCP server on port " + std::to_string(port));
        return false;
    }

    // 创建线程池和异步IO处理器
    threadPool = new ThreadPool(threadCount);
    asyncIOHandler = new AsyncIOHandler();
    asyncIOHandler->start();

    // 设置运行状态
    running.store(true);
    processedTasks.store(0);

    LogUtil::info("SyncServer started on port " + std::to_string(port) +
                  " with " + std::to_string(threadCount) + " worker threads");

    // 创建独立的accept线程，使用epoll监听客户端连接
    std::thread acceptThread([this, threadCount, port]() {
        int serverSock = tcpServer->getServerSocket();

        // 创建epoll实例
        int epFd = epoll_create1(0);
        if (epFd < 0) {
            LogUtil::error("Failed to create epoll instance for accept thread");
            return;
        }

        // 将服务器socket添加到epoll监听
        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = serverSock;
        if (epoll_ctl(epFd, EPOLL_CTL_ADD, serverSock, &ev) < 0) {
            LogUtil::error("Failed to add server socket to epoll");
            close(epFd);
            return;
        }

        const int MAX_EVENTS = 16;
        struct epoll_event events[MAX_EVENTS];

        // 主循环：等待并处理客户端连接
        while (running.load()) {
            // 等待事件，超时1秒
            int nfds = epoll_wait(epFd, events, MAX_EVENTS, 1000);
            if (nfds < 0) {
                if (errno == EINTR) continue;  // 信号中断，继续循环
                if (!running.load()) break;      // 服务器已停止
                LogUtil::error("epoll_wait error: " + std::string(std::strerror(errno)));
                continue;
            }
            if (nfds == 0) {
                continue;  // 超时，继续循环
            }

            // 处理所有就绪的事件
            for (int i = 0; i < nfds; i++) {
                // 检查是否是服务器socket的可读事件（新连接）
                if (events[i].data.fd == serverSock && (events[i].events & EPOLLIN)) {
                    int clientSock = tcpServer->accept();
                    if (clientSock < 0) {
                        if (!running.load()) break;
                        continue;
                    }

                    // 将客户端连接提交到线程池处理
                    threadPool->submit([this, clientSock]() {
                        handleClient(clientSock);
                    });
                }
            }
        }

        close(epFd);
        LogUtil::info("SyncServer accept thread stopped on port " + std::to_string(port));
    });

    // 分离线程，让其独立运行
    acceptThread.detach();

    return true;
}

/**
 * @brief 停止同步服务器
 */
void SyncServer::stop() {
    if (!running.load()) return;

    LogUtil::info("SyncServer stopping...");

    // 先停止TCP服务器（关闭监听socket，唤醒epoll_wait）
    if (tcpServer) {
        tcpServer->stop();
    }

    // 设置停止标志
    running.store(false);

    // 停止异步IO处理器和线程池
    if (asyncIOHandler) {
        asyncIOHandler->stop();
    }
    if (threadPool) {
        threadPool->stop();
    }

    LogUtil::info("SyncServer stopped. Total processed tasks: " +
                  std::to_string(processedTasks.load()));
}

/**
 * @brief 检查服务器是否正在运行
 * @return 正在运行返回true，否则返回false
 */
bool SyncServer::isRunning() const {
    return running.load();
}

/**
 * @brief 设置服务器基础目录（接收文件的根目录）
 * @param baseDir 基础目录路径
 */
void SyncServer::setBaseDirectory(const std::string& baseDir) {
    baseDirectory = baseDir;
    if (!baseDirectory.empty() && baseDirectory.back() != '/') {
        baseDirectory += '/';
    }
    LogUtil::info("SyncServer base directory set to: " + baseDirectory);
}

/**
 * @brief 获取已处理的任务数量
 * @return 任务数量
 */
size_t SyncServer::getProcessedTaskCount() const {
    return processedTasks.load();
}

/**
 * @brief 处理客户端连接
 * @param clientSock 客户端socket文件描述符
 * @return 处理成功返回true，否则返回false
 */
bool SyncServer::handleClient(int clientSock) {
    LogUtil::info("Handling client connection: fd=" + std::to_string(clientSock));

    // 接收任务数量
    int taskCount = 0;
    if (!receiveInt(clientSock, taskCount)) {
        LogUtil::error("Failed to receive task count from client");
        sendAck(clientSock, false);
        tcpServer->closeClient(clientSock);
        return false;
    }

    LogUtil::info("Receiving " + std::to_string(taskCount) + " sync tasks");

    // 接收任务元数据
    std::vector<ServerSyncTask> tasks;
    if (!receiveTaskMetadata(clientSock, taskCount, tasks)) {
        LogUtil::error("Failed to receive task metadata");
        sendAck(clientSock, false);
        tcpServer->closeClient(clientSock);
        return false;
    }

    // 处理每个任务
    for (const auto& task : tasks) {
        std::string fullDestPath = baseDirectory + task.destPath;

        if (task.isDirectory) {
            // 创建目录
            if (!LinuxFileUtil::createDirectory(fullDestPath, task.mode)) {
                LogUtil::error("Failed to create directory: " + fullDestPath);
                sendAck(clientSock, false);
                tcpServer->closeClient(clientSock);
                return false;
            }
            LogUtil::info("Created directory: " + fullDestPath);
        } else {
            // 确保目标目录存在
            size_t lastSlash = fullDestPath.find_last_of('/');
            if (lastSlash != std::string::npos) {
                std::string destDir = fullDestPath.substr(0, lastSlash);
                if (!LinuxFileUtil::createDirectory(destDir)) {
                    LogUtil::error("Failed to create directory: " + destDir);
                    sendAck(clientSock, false);
                    tcpServer->closeClient(clientSock);
                    return false;
                }
            }

            // 接收文件数据
            if (!receiveFileData(clientSock, task)) {
                LogUtil::error("Failed to receive file data: " + fullDestPath);
                sendAck(clientSock, false);
                tcpServer->closeClient(clientSock);
                return false;
            }

            // 设置文件权限
            LinuxFileUtil::setFileMode(fullDestPath, task.mode);
            LogUtil::info("Received file: " + fullDestPath +
                          " (" + std::to_string(task.fileSize) + " bytes)");
        }

        processedTasks.fetch_add(1);
    }

    // 发送成功确认
    sendAck(clientSock, true);
    tcpServer->closeClient(clientSock);
    LogUtil::info("Client connection handled successfully: " + std::to_string(tasks.size()) + " tasks");

    return true;
}

/**
 * @brief 接收任务元数据
 * @param clientSock 客户端socket文件描述符
 * @param taskCount 任务数量
 * @param tasks 任务列表（输出参数）
 * @return 接收成功返回true，否则返回false
 */
bool SyncServer::receiveTaskMetadata(int clientSock, int taskCount,
                                      std::vector<ServerSyncTask>& tasks) {
    for (int i = 0; i < taskCount; i++) {
        ServerSyncTask task;

        // 1. 接收任务类型（0=文件，1=目录）
        int taskType = 0;
        if (!receiveInt(clientSock, taskType)) {
            LogUtil::error("Failed to receive task type for task " + std::to_string(i));
            return false;
        }
        task.isDirectory = (taskType == 1);

        // 2. 接收源路径
        size_t srcPathLen = 0;
        if (!receiveSizeT(clientSock, srcPathLen)) return false;

        if (srcPathLen > 0) {
            std::vector<char> buffer(srcPathLen + 1);
            if (!receiveBytes(clientSock, buffer.data(), srcPathLen)) return false;
            buffer[srcPathLen] = '\0';
            task.srcPath = buffer.data();
        }

        // 3. 接收目标路径
        size_t destPathLen = 0;
        if (!receiveSizeT(clientSock, destPathLen)) return false;

        if (destPathLen > 0) {
            std::vector<char> buffer(destPathLen + 1);
            if (!receiveBytes(clientSock, buffer.data(), destPathLen)) return false;
            buffer[destPathLen] = '\0';
            task.destPath = buffer.data();
        }

        // 4. 接收文件权限
        if (!receiveModeT(clientSock, task.mode)) return false;

        // 5. 如果是文件，接收文件大小
        task.fileSize = 0;
        if (!task.isDirectory) {
            if (!receiveOffT(clientSock, task.fileSize)) {
                LogUtil::error("Failed to receive file size for task " + std::to_string(i));
                return false;
            }
        }

        tasks.push_back(task);
        LogUtil::info("Received task[" + std::to_string(i) + "]: " +
                      (task.isDirectory ? "DIR" : "FILE") + " -> " + task.destPath +
                      (task.isDirectory ? "" : " (" + std::to_string(task.fileSize) + " bytes)"));
    }

    return true;
}

/**
 * @brief 接收文件数据
 * @param clientSock 客户端socket文件描述符
 * @param task 同步任务（包含目标路径和文件大小）
 * @return 接收成功返回true，否则返回false
 */
bool SyncServer::receiveFileData(int clientSock, const ServerSyncTask& task) {
    std::string fullDestPath = baseDirectory + task.destPath;

    // 创建目标文件
    int destFd = open(fullDestPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (destFd < 0) {
        LogUtil::error("Failed to open destination file: " + fullDestPath +
                       " (" + std::strerror(errno) + ")");
        return false;
    }

    const size_t bufferSize = 64 * 1024;
    char buffer[bufferSize];
    off_t received = 0;

    // 循环接收文件数据
    while (received < task.fileSize) {
        size_t bytesToReceive = std::min(bufferSize,
            static_cast<size_t>(task.fileSize - received));

        // 使用健壮的read函数（处理EINTR）
        ssize_t n = robustRead(clientSock, buffer, bytesToReceive);
        if (n <= 0) {
            LogUtil::error("Failed to receive file data at offset " +
                           std::to_string(received) + " for " + fullDestPath);
            close(destFd);
            return false;
        }

        // 写入文件
        ssize_t totalWritten = 0;
        const char* ptr = buffer;
        while (totalWritten < n) {
            ssize_t written = write(destFd, ptr + totalWritten, n - totalWritten);
            if (written < 0) {
                if (errno == EINTR) continue;  // 信号中断，重试
                LogUtil::error("Failed to write file data: " + std::string(std::strerror(errno)));
                close(destFd);
                return false;
            }
            totalWritten += written;
        }

        received += n;
    }

    close(destFd);
    return true;
}

/**
 * @brief 发送确认消息给客户端
 * @param clientSock 客户端socket文件描述符
 * @param success 是否成功
 * @return 发送成功返回true，否则返回false
 */
bool SyncServer::sendAck(int clientSock, bool success) {
    const char* ack = success ? "ACK" : "NAK";
    bool sent = tcpServer->send(clientSock, ack, 3);
    if (!sent) {
        LogUtil::warn("Failed to send ACK (client may have disconnected)");
        return false;
    }
    return true;
}

/**
 * @brief 从客户端接收int类型数据
 * @param clientSock 客户端socket文件描述符
 * @param value 接收的值（输出参数）
 * @return 接收成功返回true，否则返回false
 */
bool SyncServer::receiveInt(int clientSock, int& value) {
    return receiveBytes(clientSock, &value, sizeof(int));
}

/**
 * @brief 从客户端接收size_t类型数据
 * @param clientSock 客户端socket文件描述符
 * @param value 接收的值（输出参数）
 * @return 接收成功返回true，否则返回false
 */
bool SyncServer::receiveSizeT(int clientSock, size_t& value) {
    return receiveBytes(clientSock, &value, sizeof(size_t));
}

/**
 * @brief 从客户端接收off_t类型数据
 * @param clientSock 客户端socket文件描述符
 * @param value 接收的值（输出参数）
 * @return 接收成功返回true，否则返回false
 */
bool SyncServer::receiveOffT(int clientSock, off_t& value) {
    return receiveBytes(clientSock, &value, sizeof(off_t));
}

/**
 * @brief 从客户端接收mode_t类型数据
 * @param clientSock 客户端socket文件描述符
 * @param value 接收的值（输出参数）
 * @return 接收成功返回true，否则返回false
 */
bool SyncServer::receiveModeT(int clientSock, mode_t& value) {
    return receiveBytes(clientSock, &value, sizeof(mode_t));
}

/**
 * @brief 从客户端接收指定字节数的数据
 * @param clientSock 客户端socket文件描述符
 * @param buffer 接收缓冲区
 * @param size 需要接收的字节数
 * @return 接收成功返回true，否则返回false
 */
bool SyncServer::receiveBytes(int clientSock, void* buffer, size_t size) {
    char* ptr = static_cast<char*>(buffer);
    size_t totalReceived = 0;

    while (totalReceived < size) {
        ssize_t n = robustRead(clientSock, ptr + totalReceived, size - totalReceived);
        if (n <= 0) return false;
        totalReceived += n;
    }

    return true;
}

/**
 * @brief 健壮的read函数（处理EINTR信号中断）
 * @param fd 文件描述符
 * @param buf 缓冲区
 * @param count 需要读取的字节数
 * @return 实际读取的字节数，失败返回-1
 */
ssize_t SyncServer::robustRead(int fd, void* buf, size_t count) {
    ssize_t totalRead = 0;
    char* ptr = static_cast<char*>(buf);

    while (totalRead < static_cast<ssize_t>(count)) {
        ssize_t n = read(fd, ptr + totalRead, count - totalRead);
        if (n < 0) {
            if (errno == EINTR) continue;  // 信号中断，重试
            return -1;
        }
        if (n == 0) break;  // EOF
        totalRead += n;
    }

    return totalRead;
}
