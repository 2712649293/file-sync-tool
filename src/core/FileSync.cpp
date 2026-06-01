#include "FileSync.h"
#include "LinuxFileUtil.h"
#include "LogUtil.h"
#include "ExceptionHandler.h"
#include "TcpClient.h"
#include "TcpServer.h"
#include "ThreadPool.h"
#include "AsyncIOHandler.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <thread>
#include <future>
#include <atomic>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <sys/vfs.h>
#include <sys/statvfs.h>

const off_t FileSync::LARGE_FILE_THRESHOLD;
const off_t FileSync::ASYNC_CHUNK_SIZE;

/**
 * @brief FileSync 构造函数
 * @details 初始化文件同步器，设置默认的速率限制和冲突处理策略
 */
FileSync::FileSync() : speedLimit(0), conflictStrategy("overwrite"),
    threadPool(nullptr), asyncIOHandler(nullptr) {
}

/**
 * @brief FileSync 析构函数
 * @details 清理资源
 */
FileSync::~FileSync() {
}

/**
 * @brief 收集目录的元数据
 * @details 递归收集指定目录及其子目录中所有文件的元数据
 * @param directory 要收集元数据的目录路径
 * @return 文件名到元数据的映射
 */
std::unordered_map<std::string, FileMetadata> FileSync::collectMetadata(const std::string& directory) {
    std::unordered_map<std::string, FileMetadata> metadataMap;
    // 列出目录中的所有文件和子目录
    std::vector<std::string> files = LinuxFileUtil::listDirectory(directory);
    
    // 遍历所有文件和子目录
    for (const auto& path : files) {
        struct stat statbuf;
        // 获取文件状态信息
        if (LinuxFileUtil::getFileStat(path, statbuf)) {
            FileMetadata meta;
            meta.path = path;
            meta.size = statbuf.st_size;      // 文件大小
            meta.mtime = statbuf.st_mtime;    // 修改时间
            meta.mode = statbuf.st_mode;      // 文件权限和类型
            
            // 对于普通文件，计算MD5值
            if (S_ISREG(statbuf.st_mode)) {
                meta.md5 = LinuxFileUtil::calculateMD5(path);
            }
            
            // 将元数据添加到映射中
            metadataMap[path] = meta;
            
            // 递归处理子目录
            if (S_ISDIR(statbuf.st_mode)) {
                auto subMetadata = collectMetadata(path);
                metadataMap.insert(subMetadata.begin(), subMetadata.end());
            }
        }
    }
    
    return metadataMap;
}

/**
 * @brief 比较源目录和目标目录的元数据，生成同步任务
 * @param srcMetadata 源目录的元数据
 * @param destMetadata 目标目录的元数据
 * @param srcDir 源目录路径
 * @param destDir 目标目录路径
 * @return 同步任务列表
 */
std::vector<SyncTask> FileSync::compareMetadata(
    const std::unordered_map<std::string, FileMetadata>& srcMetadata,
    const std::unordered_map<std::string, FileMetadata>& destMetadata,
    const std::string& srcDir,
    const std::string& destDir
) {
    std::vector<SyncTask> tasks;
    
    // 检查源目录中的文件
    for (const auto& pair : srcMetadata) {
        const std::string& srcPath = pair.first;
        const FileMetadata& srcMeta = pair.second;
        
        // 构建目标路径
        //substr(size_type _Off, size_type _Count)从_Off开始提取_Count个字符，_Count表示提取多长，没有就是默认提取到字符串结束
        std::string destPath = destDir + srcPath.substr(srcDir.length());
        
        // 检查目标目录中是否存在该文件
        auto destIt = destMetadata.find(destPath);
        if (destIt == destMetadata.end()) {
            // 文件不存在，需要新建
            SyncTask task;
            task.srcPath = srcPath;
            task.destPath = destPath;
            task.isDirectory = S_ISDIR(srcMeta.mode);
            task.mode = srcMeta.mode;
            tasks.push_back(task);
        } else {
            // 文件存在，检查是否需要更新
            const FileMetadata& destMeta = destIt->second;
            if (needSync(srcMeta, destMeta)) {
                SyncTask task;
                task.srcPath = srcPath;
                task.destPath = destPath;
                task.isDirectory = S_ISDIR(srcMeta.mode);
                task.mode = srcMeta.mode;
                tasks.push_back(task);
            }
        }
    }
    
    // 检查目标目录中需要删除的文件（可选）
    // 这里可以根据需要添加删除逻辑
    
    return tasks;
}

/**
 * @brief 判断两个文件是否需要同步
 * @param srcMeta 源文件的元数据
 * @param destMeta 目标文件的元数据
 * @return 如果需要同步返回true，否则返回false
 */
bool FileSync::needSync(const FileMetadata& srcMeta, const FileMetadata& destMeta) {
    // 检查文件类型是否相同
    if (S_ISREG(srcMeta.mode) != S_ISREG(destMeta.mode)) {
        return true;
    }
    
    // 检查大小和修改时间
    if (srcMeta.size != destMeta.size || srcMeta.mtime != destMeta.mtime) {
        return true;
    }
    
    // 对于普通文件，检查MD5
    if (S_ISREG(srcMeta.mode)) {
        return srcMeta.md5 != destMeta.md5;
    }
    
    return false;
}

/**
 * @brief 检测源目录和目标目录之间的变化
 * @param srcDir 源目录路径
 * @param destDir 目标目录路径
 * @return 同步任务列表
 */
std::vector<SyncTask> FileSync::detectChanges(const std::string& srcDir, const std::string& destDir) {
    LogUtil::info("Collecting metadata for source directory: " + srcDir);
    auto srcMetadata = collectMetadata(srcDir);
    
    LogUtil::info("Collecting metadata for destination directory: " + destDir);
    auto destMetadata = collectMetadata(destDir);
    
    LogUtil::info("Comparing metadata and generating sync tasks");
    return compareMetadata(srcMetadata, destMetadata, srcDir, destDir);
}

/**
 * @brief 执行本地同步
 * @param srcDir 源目录路径
 * @param destDir 目标目录路径
 * @param recursive 是否递归同步
 * @return 同步成功返回true，否则返回false
 */
bool FileSync::syncLocal(const std::string& srcDir, const std::string& destDir, bool recursive) {
    LogUtil::info("Starting local sync: " + srcDir + " -> " + destDir);
    
    // 确保目标目录存在
    if (!LinuxFileUtil::createDirectory(destDir)) {
        LogUtil::error("Failed to create destination directory: " + destDir);
        return false;
    }
    
    // 检测变化
    auto tasks = detectChanges(srcDir, destDir);
    
    LogUtil::info("Found " + std::to_string(tasks.size()) + " tasks to sync");
    
    // 执行同步任务
    for (const auto& task : tasks) {
        if (!executeSyncTask(task)) {
            LogUtil::error("Failed to execute sync task for: " + task.srcPath);
            return false;
        }
    }
    
    LogUtil::info("Local sync completed successfully");
    return true;
}

/**
 * @brief 执行远程同步
 * @param srcDir 源目录路径
 * @param destDir 目标目录路径
 * @param host 远程主机IP地址
 * @param port 远程主机端口
 * @param resume 是否启用断点续传
 * @return 同步成功返回true，否则返回false
 */
bool FileSync::syncRemote(const std::string& srcDir, const std::string& destDir, const std::string& host, int port, bool resume) {
    LogUtil::info("Starting remote sync" + std::string(resume ? " (resume enabled)" : "") +
                  ": " + srcDir + " -> " + host + ":" + std::to_string(port) + destDir);

    TcpClient client;
    if (!client.connect(host, port)) {
        LogUtil::error("Failed to connect to remote server");
        return false;
    }

    auto tasks = detectChanges(srcDir, destDir);

    LogUtil::info("Found " + std::to_string(tasks.size()) + " tasks to sync");

    int taskCount = tasks.size();
    client.send(&taskCount, sizeof(taskCount));

    for (const auto& task : tasks) {
        int taskType = task.isDirectory ? 1 : 0;
        client.send(&taskType, sizeof(taskType));

        size_t srcPathLen = task.srcPath.length();
        client.send(&srcPathLen, sizeof(srcPathLen));
        client.send(task.srcPath.c_str(), srcPathLen);

        size_t destPathLen = task.destPath.length();
        client.send(&destPathLen, sizeof(destPathLen));
        client.send(task.destPath.c_str(), destPathLen);

        client.send(&task.mode, sizeof(task.mode));

        if (!task.isDirectory) {
            struct stat statbuf;
            if (LinuxFileUtil::getFileStat(task.srcPath, statbuf)) {
                off_t fileSize = statbuf.st_size;
                client.send(&fileSize, sizeof(fileSize));
            }
        }
    }

    for (const auto& task : tasks) {
        if (!task.isDirectory) {
            struct stat statbuf;
            off_t fileSize = 0;
            if (LinuxFileUtil::getFileStat(task.srcPath, statbuf)) {
                fileSize = statbuf.st_size;
            }

            off_t resumeOffset = 0;
            if (resume) {
                SyncProgress progress = loadProgress(task.destPath);
                if (progress.offset > 0 && progress.totalSize > 0 && progress.offset < fileSize) {
                    resumeOffset = progress.offset;
                    LogUtil::info("Resuming remote file from offset " + std::to_string(resumeOffset) +
                                  " (" + task.srcPath + ", " +
                                  std::to_string(progress.offset) + "/" + std::to_string(progress.totalSize) + ")");
                }
            }

            client.send(&resumeOffset, sizeof(resumeOffset));

            bool useAsync = (fileSize >= LARGE_FILE_THRESHOLD && asyncIOHandler && asyncIOHandler->isRunning());

            if (resumeOffset > 0 && useAsync) {
                LogUtil::info("Resume with fallback to sync send for: " + task.srcPath);
                useAsync = false;
            }

            if (useAsync) {
                LogUtil::info("Using async IO for large file: " + task.srcPath);
                if (!sendFileViaAsyncIO(task.srcPath, client.getSocketFd())) {
                    LogUtil::error("Async IO transfer failed, falling back to sync: " + task.srcPath);
                    if (!sendFileSync(task.srcPath, task.destPath, client, resumeOffset)) {
                        client.disconnect();
                        return false;
                    }
                }
            } else {
                if (!sendFileSync(task.srcPath, task.destPath, client, resumeOffset)) {
                    client.disconnect();
                    return false;
                }
            }
        } else {
            off_t zeroResume = 0;
            client.send(&zeroResume, sizeof(zeroResume));
        }
    }

    char ackBuffer[4] = {0};
    size_t ackReceived = client.receive(ackBuffer, 3);
    client.disconnect();

    if (ackReceived == 3 && strncmp(ackBuffer, "ACK", 3) == 0) {
        LogUtil::info("Server acknowledged successful sync");
        return true;
    }

    LogUtil::error("Server returned error or no acknowledgment");
    return false;
}

/**
 * @brief 执行同步任务
 * @param task 同步任务
 * @return 执行成功返回true，否则返回false
 */
bool FileSync::executeSyncTask(const SyncTask& task) {
    if (task.isDirectory) {
        return syncDirectory(task.srcPath, task.destPath, task.mode);
    } else {
        return syncFile(task.srcPath, task.destPath, task.mode);
    }
}

/**
 * @brief 同步目录
 * @param srcPath 源目录路径
 * @param destPath 目标目录路径
 * @param mode 目录权限
 * @return 同步成功返回true，否则返回false
 */
bool FileSync::syncDirectory(const std::string& srcPath, const std::string& destPath, mode_t mode) {
    LogUtil::info("Syncing directory: " + srcPath + " -> " + destPath);
    
    // 创建目标目录
    if (!LinuxFileUtil::createDirectory(destPath)) {
        LogUtil::error("Failed to create directory: " + destPath);
        return false;
    }
    
    // 设置目录权限
    if (!LinuxFileUtil::setFileMode(destPath, mode)) {
        LogUtil::error("Failed to set directory permissions: " + destPath);
        return false;
    }
    
    return true;
}

/**
 * @brief 同步文件
 * @param srcPath 源文件路径
 * @param destPath 目标文件路径
 * @param mode 文件权限
 * @return 同步成功返回true，否则返回false
 */
bool FileSync::syncFile(const std::string& srcPath, const std::string& destPath, mode_t mode) {
    LogUtil::info("Syncing file: " + srcPath + " -> " + destPath);
    
    // 检查源文件是否存在
    if (!LinuxFileUtil::fileExists(srcPath)) {
        LogUtil::error("Source file does not exist: " + srcPath);
        return false;
    }
    
    // 确保目标目录存在
    size_t lastSlash = destPath.find_last_of('/');
    if (lastSlash != std::string::npos) {
        std::string destDir = destPath.substr(0, lastSlash);
        if (!LinuxFileUtil::createDirectory(destDir)) {
            LogUtil::error("Failed to create destination directory: " + destDir);
            return false;
        }
    }
    
    // 处理文件冲突
    if (LinuxFileUtil::fileExists(destPath)) {
        if (!handleConflict(srcPath, destPath)) {
            return false;
        }
    }
    
    // 尝试断点续传
    SyncProgress progress = loadProgress(destPath);
    if (progress.offset > 0 && progress.totalSize > 0) {
        LogUtil::info("Resuming sync from offset: " + std::to_string(progress.offset));
        bool resumeOk = transferFile(srcPath, destPath, progress.offset);
        if (resumeOk) {
            std::remove(getProgressFilePath(destPath).c_str());
            LinuxFileUtil::setFileMode(destPath, mode);
            return true;
        }
    }

    // 获取文件大小，判断是否使用异步并行传输
    struct stat statbuf;
    bool useAsync = false;
    if (LinuxFileUtil::getFileStat(srcPath, statbuf)) {
        if (statbuf.st_size >= LARGE_FILE_THRESHOLD && threadPool) {
            useAsync = true;
        }
    }

    if (useAsync) {
        if (transferLargeFileAsync(srcPath, destPath, 0)) {
            LinuxFileUtil::setFileMode(destPath, mode);
            return true;
        }
        cleanupCorruptedFile(destPath);
    }

    // 正常传输（小文件或异步不可用的回退方案）
    if (transferFile(srcPath, destPath)) {
        LinuxFileUtil::setFileMode(destPath, mode);
        return true;
    }

    cleanupCorruptedFile(destPath);
    return false;
}

/**
 * @brief 传输文件
 * @param srcPath 源文件路径
 * @param destPath 目标文件路径
 * @param offset 偏移量（用于断点续传）
 * @return 传输成功返回true，否则返回false
 */
bool FileSync::transferFile(const std::string& srcPath, const std::string& destPath, off_t offset) {
    // 打开源文件和目标文件
    std::ifstream srcFile(srcPath, std::ios::binary);
    std::ofstream destFile(destPath, std::ios::binary | (offset > 0 ? std::ios::app : std::ios::trunc));
    
    if (!srcFile || !destFile) {
        LogUtil::error("Failed to open files for transfer");
        return false;
    }
    
    // 定位到偏移位置（断点续传）
    if (offset > 0) {
        srcFile.seekg(offset);
        if (!srcFile) {
            LogUtil::error("Failed to seek in source file");
            return false;
        }
    }
    
    // 获取文件总大小
    srcFile.seekg(0, std::ios::end);
    off_t totalSize = srcFile.tellg();
    srcFile.seekg(offset);
    
    // 传输数据
    const size_t bufferSize = 4096;
    char buffer[bufferSize];
    off_t transferred = offset;
    
    // 循环读取并写入数据
    while (srcFile.read(buffer, bufferSize)) {
        size_t bytesRead = srcFile.gcount();
        destFile.write(buffer, bytesRead);
        
        if (!destFile) {
            LogUtil::error("Failed to write to destination file");
            // 保存进度
            saveProgress(destPath, transferred, totalSize);
            return false;
        }
        
        transferred += bytesRead;
        
        // 保存进度
        saveProgress(destPath, transferred, totalSize);
        
        // 速率限制
        if (speedLimit > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(bufferSize * 1000 / speedLimit));
        }
    }
    
    // 处理最后一次读取
    if (srcFile.gcount() > 0) {
        destFile.write(buffer, srcFile.gcount());
        transferred += srcFile.gcount();
    }
    
    std::remove(getProgressFilePath(destPath).c_str());
    return true;
}

/**
 * @brief 从断点处续传文件
 * @param srcPath 源文件路径
 * @param destPath 目标文件路径
 * @return 续传成功返回true，否则返回false
 */
bool FileSync::resumeSync(const std::string& srcPath, const std::string& destPath) {
    SyncProgress progress = loadProgress(destPath);
    if (progress.offset > 0 && progress.totalSize > 0) {
        return transferFile(srcPath, destPath, progress.offset);
    }
    return false;
}

/**
 * @brief 保存同步进度
 * @param filePath 文件路径
 * @param offset 当前偏移量
 * @param totalSize 文件总大小
 */
void FileSync::saveProgress(const std::string& filePath, off_t offset, off_t totalSize) {
    std::string progressPath = getProgressFilePath(filePath);
    std::ofstream progressFile(progressPath);
    if (progressFile) {
        progressFile << filePath << "\n";
        progressFile << offset << "\n";
        progressFile << totalSize << "\n";
    }
}

/**
 * @brief 加载同步进度
 * @param filePath 文件路径
 * @return 同步进度信息
 */
SyncProgress FileSync::loadProgress(const std::string& filePath) {
    SyncProgress progress;
    progress.filePath = filePath;
    progress.offset = 0;
    progress.totalSize = 0;
    
    std::string progressPath = getProgressFilePath(filePath);
    std::ifstream progressFile(progressPath);
    if (progressFile) {
        std::string savedPath;
        progressFile >> savedPath;
        if (savedPath == filePath) {
            progressFile >> progress.offset;
            progressFile >> progress.totalSize;
        }
    }
    
    return progress;
}

/**
 * @brief 获取进度文件路径
 * @param filePath 文件路径
 * @return 进度文件路径
 */
std::string FileSync::getProgressFilePath(const std::string& filePath) {
    return filePath + ".sync_progress";
}

/**
 * @brief 处理文件冲突
 * @param srcPath 源文件路径
 * @param destPath 目标文件路径
 * @return 处理成功返回true，否则返回false
 */
bool FileSync::handleConflict(const std::string& srcPath, const std::string& destPath) {
    if (conflictStrategy == "overwrite") {
        LogUtil::info("Overwriting existing file: " + destPath);
        return true;
    } else if (conflictStrategy == "skip") {
        LogUtil::info("Skipping existing file: " + destPath);
        return false;
    } else if (conflictStrategy == "rename") {
        std::string newPath = destPath + ".bak";
        LogUtil::info("Renaming existing file to: " + newPath);
        if (std::rename(destPath.c_str(), newPath.c_str()) != 0) {
            LogUtil::error("Failed to rename existing file");
            return false;
        }
        return true;
    }
    return true;
}

/**
 * @brief 计算文件的MD5值
 * @param path 文件路径
 * @return MD5值
 */
std::string FileSync::calculateFileMD5(const std::string& path) {
    return LinuxFileUtil::calculateMD5(path);
}

/**
 * @brief 设置速率限制
 * @param limit 速率限制（字节/秒）
 */
void FileSync::setSpeedLimit(size_t limit) {
    speedLimit = limit;
}

/**
 * @brief 设置冲突处理策略
 * @param strategy 冲突处理策略（overwrite/skip/rename）
 */
void FileSync::setConflictStrategy(const std::string& strategy) {
    conflictStrategy = strategy;
}

void FileSync::setThreadPool(ThreadPool* pool) {
    threadPool = pool;
}

void FileSync::setAsyncIOHandler(AsyncIOHandler* handler) {
    asyncIOHandler = handler;
}

bool FileSync::sendFileSync(const std::string& srcPath, const std::string& destPath, TcpClient& client, off_t resumeOffset) {
    std::ifstream srcFile(srcPath, std::ios::binary);
    if (!srcFile) {
        LogUtil::error("Failed to open source file: " + srcPath);
        return false;
    }

    srcFile.seekg(0, std::ios::end);
    off_t fileSize = srcFile.tellg();
    srcFile.seekg(resumeOffset);

    const size_t bufferSize = 4096;
    char buffer[bufferSize];
    off_t transferred = resumeOffset;

    while (transferred < fileSize) {
        size_t bytesToRead = std::min(bufferSize, static_cast<size_t>(fileSize - transferred));
        srcFile.read(buffer, bytesToRead);
        size_t bytesRead = srcFile.gcount();

        if (bytesRead > 0) {
            if (!client.send(buffer, bytesRead)) {
                LogUtil::error("Failed to send file data: " + srcPath);
                saveProgress(destPath, transferred, fileSize);
                return false;
            }
            transferred += bytesRead;
            saveProgress(destPath, transferred, fileSize);

            if (speedLimit > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(bufferSize * 1000 / speedLimit));
            }
        }
    }

    std::remove(getProgressFilePath(destPath).c_str());
    return true;
}

bool FileSync::transferLargeFileAsync(const std::string& srcPath, const std::string& destPath, off_t offset) {
    struct stat statbuf;
    if (!LinuxFileUtil::getFileStat(srcPath, statbuf)) {
        LogUtil::error("Failed to get file stat: " + srcPath);
        return false;
    }
    off_t fileSize = statbuf.st_size;
    off_t totalSize = fileSize;

    if (!checkDiskSpace(destPath, fileSize - offset)) {
        LogUtil::error("Insufficient disk space for: " + destPath);
        return false;
    }

    LogUtil::info("Starting async parallel transfer: " + srcPath +
                  " (" + std::to_string(fileSize) + " bytes, " +
                  std::to_string((fileSize - offset + ASYNC_CHUNK_SIZE - 1) / ASYNC_CHUNK_SIZE) + " chunks)");

    int srcFd = open(srcPath.c_str(), O_RDONLY);
    if (srcFd < 0) {
        LogUtil::error("Failed to open source file: " + srcPath + " (" + std::strerror(errno) + ")");
        return false;
    }

    int destFd = -1;
    if (offset == 0) {
        destFd = open(destPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (destFd < 0) {
            LogUtil::error("Failed to create destination file: " + destPath + " (" + std::strerror(errno) + ")");
            close(srcFd);
            return false;
        }
        if (ftruncate(destFd, fileSize) != 0) {
            LogUtil::error("Failed to pre-allocate destination file: " + destPath);
            close(destFd);
            close(srcFd);
            cleanupCorruptedFile(destPath);
            return false;
        }
    } else {
        destFd = open(destPath.c_str(), O_WRONLY);
        if (destFd < 0) {
            LogUtil::error("Failed to open destination file for resume: " + destPath + " (" + std::strerror(errno) + ")");
            close(srcFd);
            return false;
        }
    }

    int numChunks = (fileSize - offset + ASYNC_CHUNK_SIZE - 1) / ASYNC_CHUNK_SIZE;
    std::vector<std::future<bool>> futures;

    auto transferred = std::make_shared<std::atomic<off_t>>(offset);
    auto hasError = std::make_shared<std::atomic<bool>>(false);

    for (int i = 0; i < numChunks; i++) {
        off_t chunkOffset = offset + i * ASYNC_CHUNK_SIZE;
        off_t currentChunkSize = std::min(ASYNC_CHUNK_SIZE, fileSize - chunkOffset);

        futures.push_back(std::async(std::launch::async,
            [this, destPath, srcFd, destFd, chunkOffset, currentChunkSize, totalSize,
             transferred, hasError]() -> bool {
            if (hasError->load()) return false;

            thread_local std::vector<char> tlBuffer;
            if (tlBuffer.size() < static_cast<size_t>(ASYNC_CHUNK_SIZE)) {
                tlBuffer.resize(ASYNC_CHUNK_SIZE);
            }

            bool ok = transferChunk(srcFd, destFd, chunkOffset, currentChunkSize,
                                    tlBuffer.data(), tlBuffer.size());
            if (!ok) {
                hasError->store(true);
                return false;
            }

            off_t newTransferred = transferred->fetch_add(currentChunkSize) + currentChunkSize;
            saveProgress(destPath, newTransferred, totalSize);

            if (speedLimit > 0) {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(currentChunkSize * 1000 / speedLimit / 4));
            }

            return true;
        }));
    }

    bool allSuccess = true;
    for (auto& f : futures) {
        if (!f.get()) {
            allSuccess = false;
        }
    }

    close(srcFd);
    close(destFd);

    if (!allSuccess) {
        LogUtil::error("Async parallel transfer failed for: " + srcPath);
        cleanupCorruptedFile(destPath);
        return false;
    }

    std::remove(getProgressFilePath(destPath).c_str());
    LogUtil::info("Async parallel transfer completed: " + srcPath);
    return true;
}

bool FileSync::transferChunk(int srcFd, int destFd, off_t chunkOffset, off_t chunkSize,
                              char* buffer, size_t bufferSize) {
    if (bufferSize < static_cast<size_t>(chunkSize)) {
        LogUtil::error("Buffer too small for chunk transfer");
        return false;
    }

    ssize_t bytesRead = robustPread(srcFd, buffer, chunkSize, chunkOffset);
    if (bytesRead != chunkSize) {
        LogUtil::error("Failed to read chunk at offset " + std::to_string(chunkOffset) +
                       " (" + std::strerror(errno) + ")");
        return false;
    }

    ssize_t bytesWritten = robustPwrite(destFd, buffer, chunkSize, chunkOffset);
    if (bytesWritten != chunkSize) {
        LogUtil::error("Failed to write chunk at offset " + std::to_string(chunkOffset) +
                       " (" + std::strerror(errno) + ")");
        return false;
    }

    return true;
}

ssize_t FileSync::robustPread(int fd, void* buf, size_t count, off_t offset) {
    ssize_t totalRead = 0;
    char* ptr = static_cast<char*>(buf);
    while (totalRead < static_cast<ssize_t>(count)) {
        ssize_t n = pread(fd, ptr + totalRead, count - totalRead, offset + totalRead);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) break;
        totalRead += n;
    }
    return totalRead;
}

ssize_t FileSync::robustPwrite(int fd, const void* buf, size_t count, off_t offset) {
    ssize_t totalWritten = 0;
    const char* ptr = static_cast<const char*>(buf);
    while (totalWritten < static_cast<ssize_t>(count)) {
        ssize_t n = pwrite(fd, ptr + totalWritten, count - totalWritten, offset + totalWritten);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) break;
        totalWritten += n;
    }
    return totalWritten;
}

bool FileSync::checkDiskSpace(const std::string& destPath, off_t requiredSize) {
    struct statvfs vfs;
    size_t lastSlash = destPath.find_last_of('/');
    std::string dirPath = (lastSlash != std::string::npos) ? destPath.substr(0, lastSlash) : ".";
    if (statvfs(dirPath.c_str(), &vfs) != 0) {
        LogUtil::error("Failed to check disk space for: " + dirPath);
        return false;
    }
    off_t available = static_cast<off_t>(vfs.f_bavail) * vfs.f_bsize;
    if (available < requiredSize) {
        LogUtil::error("Insufficient disk space: need " + std::to_string(requiredSize) +
                       " bytes, available " + std::to_string(available) + " bytes");
        return false;
    }
    return true;
}

void FileSync::cleanupCorruptedFile(const std::string& destPath) {
    if (LinuxFileUtil::fileExists(destPath)) {
        LogUtil::info("Cleaning up corrupted file: " + destPath);
        std::remove(destPath.c_str());
    }
    std::string progressPath = getProgressFilePath(destPath);
    if (LinuxFileUtil::fileExists(progressPath)) {
        std::remove(progressPath.c_str());
    }
}

bool FileSync::sendFileViaAsyncIO(const std::string& srcPath, int sockfd) {
    if (!asyncIOHandler || !asyncIOHandler->isRunning()) {
        LogUtil::error("AsyncIOHandler is not available for async network transfer");
        return false;
    }

    struct stat statbuf;
    if (!LinuxFileUtil::getFileStat(srcPath, statbuf)) return false;

    std::ifstream srcFile(srcPath, std::ios::binary);
    if (!srcFile) return false;

    std::atomic<bool> transferDone(false);
    std::atomic<bool> transferError(false);
    std::mutex cvMutex;
    std::condition_variable cv;

    const size_t bufferSize = 8192;
    char* buffer = new char[bufferSize];

    srcFile.read(buffer, bufferSize);
    size_t bytesRead = srcFile.gcount();

    if (bytesRead > 0) {
        asyncIOHandler->registerWrite(sockfd, buffer, bytesRead,
            [&transferDone, &transferError, &cv, buffer, &srcFile, sockfd, this, &bufferSize](int fd) {
            delete[] buffer;

            if (srcFile.eof()) {
                transferDone.store(true);
                cv.notify_one();
                return;
            }

            char* nextBuffer = new char[bufferSize];
            srcFile.read(nextBuffer, bufferSize);
            size_t nextBytes = srcFile.gcount();

            if (nextBytes > 0) {
                this->asyncIOHandler->registerWrite(fd, nextBuffer, nextBytes,
                    [&transferDone, &transferError, &cv](int) {
                    transferDone.store(true);
                    cv.notify_one();
                });
            } else {
                delete[] nextBuffer;
                transferDone.store(true);
                cv.notify_one();
            }
        });

        std::unique_lock<std::mutex> lock(cvMutex);
        cv.wait(lock, [&transferDone]() { return transferDone.load(); });
    } else {
        delete[] buffer;
    }

    return !transferError.load();
}
