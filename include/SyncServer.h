#ifndef SYNC_SERVER_H
#define SYNC_SERVER_H

#include <string>
#include <vector>
#include <atomic>
#include <sys/stat.h>

class TcpServer;
class ThreadPool;
class AsyncIOHandler;

struct ServerSyncTask {
    std::string destPath;
    std::string srcPath;
    bool isDirectory;
    mode_t mode;
    off_t fileSize;
    off_t resumeOffset;
};

class SyncServer {
public:
    SyncServer();
    ~SyncServer();

    bool start(int port, size_t threadCount = 8);

    void stop();

    bool isRunning() const;

    void setBaseDirectory(const std::string& baseDir);

    size_t getProcessedTaskCount() const;

private:
    bool handleClient(int clientSock);

    bool receiveTaskMetadata(int clientSock, int taskCount,
                             std::vector<ServerSyncTask>& tasks);

    bool receiveFileData(int clientSock, const ServerSyncTask& task, off_t resumeOffset = 0);

    bool sendAck(int clientSock, bool success);

    bool receiveInt(int clientSock, int& value);

    bool receiveSizeT(int clientSock, size_t& value);

    bool receiveOffT(int clientSock, off_t& value);

    bool receiveBytes(int clientSock, void* buffer, size_t size);

    bool receiveModeT(int clientSock, mode_t& value);

    static ssize_t robustRead(int fd, void* buf, size_t count);

    TcpServer* tcpServer;
    ThreadPool* threadPool;
    AsyncIOHandler* asyncIOHandler;

    std::string baseDirectory;

    std::atomic<bool> running;
    std::atomic<size_t> processedTasks;
};

#endif