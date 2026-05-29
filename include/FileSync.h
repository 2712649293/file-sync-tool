#ifndef FILE_SYNC_H
#define FILE_SYNC_H

#include <string>
#include <vector>
#include <unordered_map>
#include <sys/stat.h>

class ThreadPool;
class AsyncIOHandler;

struct FileMetadata {
    std::string path;
    off_t size;
    time_t mtime;
    mode_t mode;
    std::string md5;
};

struct SyncTask {
    std::string srcPath;
    std::string destPath;
    bool isDirectory;
    mode_t mode;
};

struct SyncProgress {
    std::string filePath;
    off_t offset;
    off_t totalSize;
};

class FileSync {
public:
    FileSync();
    ~FileSync();

    bool syncLocal(const std::string& srcDir, const std::string& destDir, bool recursive = true);

    bool syncRemote(const std::string& srcDir, const std::string& destDir, const std::string& host, int port);

    std::vector<SyncTask> detectChanges(const std::string& srcDir, const std::string& destDir);

    bool executeSyncTask(const SyncTask& task);

    bool resumeSync(const std::string& srcPath, const std::string& destPath);

    void saveProgress(const std::string& filePath, off_t offset, off_t totalSize);

    SyncProgress loadProgress(const std::string& filePath);

    void setSpeedLimit(size_t limit);

    void setConflictStrategy(const std::string& strategy);

    void setThreadPool(ThreadPool* pool);

    void setAsyncIOHandler(AsyncIOHandler* handler);

    static const off_t LARGE_FILE_THRESHOLD = 10 * 1024 * 1024;

    static const off_t ASYNC_CHUNK_SIZE = 4 * 1024 * 1024;

private:
    std::unordered_map<std::string, FileMetadata> collectMetadata(const std::string& directory);

    std::vector<SyncTask> compareMetadata(
        const std::unordered_map<std::string, FileMetadata>& srcMetadata,
        const std::unordered_map<std::string, FileMetadata>& destMetadata,
        const std::string& srcDir,
        const std::string& destDir
    );

    bool syncFile(const std::string& srcPath, const std::string& destPath, mode_t mode);

    bool syncDirectory(const std::string& srcPath, const std::string& destPath, mode_t mode);

    bool transferFile(const std::string& srcPath, const std::string& destPath, off_t offset = 0);

    bool transferLargeFileAsync(const std::string& srcPath, const std::string& destPath, off_t offset);

    bool transferChunk(int srcFd, int destFd, off_t chunkOffset, off_t chunkSize,
                       char* buffer, size_t bufferSize);

    static ssize_t robustPread(int fd, void* buf, size_t count, off_t offset);

    static ssize_t robustPwrite(int fd, const void* buf, size_t count, off_t offset);

    bool checkDiskSpace(const std::string& destPath, off_t requiredSize);

    void cleanupCorruptedFile(const std::string& destPath);

    bool sendFileViaAsyncIO(const std::string& srcPath, int sockfd);

    bool sendFileSync(const std::string& srcPath, class TcpClient& client);

    std::string calculateFileMD5(const std::string& path);

    bool needSync(const FileMetadata& srcMeta, const FileMetadata& destMeta);

    bool handleConflict(const std::string& srcPath, const std::string& destPath);

    std::string getProgressFilePath(const std::string& filePath);

    size_t speedLimit;

    std::string conflictStrategy;

    ThreadPool* threadPool;

    AsyncIOHandler* asyncIOHandler;
};

#endif // FILE_SYNC_H
