#include "FileSync.h"
#include "LinuxFileUtil.h"
#include "LogUtil.h"
#include "ExceptionHandler.h"
#include "TcpClient.h"
#include "TcpServer.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <thread>

FileSync::FileSync() : speedLimit(0), conflictStrategy("overwrite") {
}

FileSync::~FileSync() {
}

std::unordered_map<std::string, FileMetadata> FileSync::collectMetadata(const std::string& directory) {
    std::unordered_map<std::string, FileMetadata> metadataMap;
    std::vector<std::string> files = LinuxFileUtil::listDirectory(directory);
    
    for (const auto& path : files) {
        struct stat statbuf;
        if (LinuxFileUtil::getFileStat(path, statbuf)) {
            FileMetadata meta;
            meta.path = path;
            meta.size = statbuf.st_size;
            meta.mtime = statbuf.st_mtime;
            meta.mode = statbuf.st_mode;
            
            if (S_ISREG(statbuf.st_mode)) {
                meta.md5 = LinuxFileUtil::calculateMD5(path);
            }
            
            metadataMap[path] = meta;
            
            if (S_ISDIR(statbuf.st_mode)) {
                auto subMetadata = collectMetadata(path);
                metadataMap.insert(subMetadata.begin(), subMetadata.end());
            }
        }
    }
    
    return metadataMap;
}

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

std::vector<SyncTask> FileSync::detectChanges(const std::string& srcDir, const std::string& destDir) {
    LogUtil::info("Collecting metadata for source directory: " + srcDir);
    auto srcMetadata = collectMetadata(srcDir);
    
    LogUtil::info("Collecting metadata for destination directory: " + destDir);
    auto destMetadata = collectMetadata(destDir);
    
    LogUtil::info("Comparing metadata and generating sync tasks");
    return compareMetadata(srcMetadata, destMetadata, srcDir, destDir);
}

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

bool FileSync::syncRemote(const std::string& srcDir, const std::string& destDir, const std::string& host, int port) {
    LogUtil::info("Starting remote sync: " + srcDir + " -> " + host + ":" + std::to_string(port) + destDir);
    
    // 这里需要实现远程同步逻辑
    // 1. 连接到远程服务器
    // 2. 发送同步任务列表
    // 3. 传输文件数据
    // 4. 处理断点续传
    
    TcpClient client;
    if (!client.connect(host, port)) {
        LogUtil::error("Failed to connect to remote server");
        return false;
    }
    
    // 检测变化
    auto tasks = detectChanges(srcDir, destDir);
    
    LogUtil::info("Found " + std::to_string(tasks.size()) + " tasks to sync");
    
    // 发送任务数量
    int taskCount = tasks.size();
    client.send(&taskCount, sizeof(taskCount));
    
    // 发送任务列表
    for (const auto& task : tasks) {
        // 发送任务信息
        // 这里需要实现具体的通信协议
    }
    
    // 传输文件
    for (const auto& task : tasks) {
        if (!executeSyncTask(task)) {
            LogUtil::error("Failed to execute sync task for: " + task.srcPath);
            client.disconnect();
            return false;
        }
    }
    
    client.disconnect();
    LogUtil::info("Remote sync completed successfully");
    return true;
}

bool FileSync::executeSyncTask(const SyncTask& task) {
    if (task.isDirectory) {
        return syncDirectory(task.srcPath, task.destPath, task.mode);
    } else {
        return syncFile(task.srcPath, task.destPath, task.mode);
    }
}

bool FileSync::syncDirectory(const std::string& srcPath, const std::string& destPath, mode_t mode) {
    LogUtil::info("Syncing directory: " + srcPath + " -> " + destPath);
    
    if (!LinuxFileUtil::createDirectory(destPath)) {
        LogUtil::error("Failed to create directory: " + destPath);
        return false;
    }
    
    if (!LinuxFileUtil::setFileMode(destPath, mode)) {
        LogUtil::error("Failed to set directory permissions: " + destPath);
        return false;
    }
    
    return true;
}

bool FileSync::syncFile(const std::string& srcPath, const std::string& destPath, mode_t mode) {
    LogUtil::info("Syncing file: " + srcPath + " -> " + destPath);
    
    // 检查文件是否存在
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
        if (transferFile(srcPath, destPath, progress.offset)) {
            // 同步完成，删除进度文件
            std::remove(getProgressFilePath(destPath).c_str());
            // 设置文件权限
            LinuxFileUtil::setFileMode(destPath, mode);
            return true;
        }
    }
    
    // 正常传输
    if (transferFile(srcPath, destPath)) {
        // 设置文件权限
        LinuxFileUtil::setFileMode(destPath, mode);
        return true;
    }
    
    return false;
}

bool FileSync::transferFile(const std::string& srcPath, const std::string& destPath, off_t offset) {
    std::ifstream srcFile(srcPath, std::ios::binary);
    std::ofstream destFile(destPath, std::ios::binary | (offset > 0 ? std::ios::app : std::ios::trunc));
    
    if (!srcFile || !destFile) {
        LogUtil::error("Failed to open files for transfer");
        return false;
    }
    
    // 定位到偏移位置
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
        saveProgress(destPath, transferred, totalSize);
    }
    
    return true;
}

bool FileSync::resumeSync(const std::string& srcPath, const std::string& destPath) {
    SyncProgress progress = loadProgress(destPath);
    if (progress.offset > 0 && progress.totalSize > 0) {
        return transferFile(srcPath, destPath, progress.offset);
    }
    return false;
}

void FileSync::saveProgress(const std::string& filePath, off_t offset, off_t totalSize) {
    std::string progressPath = getProgressFilePath(filePath);
    std::ofstream progressFile(progressPath);
    if (progressFile) {
        progressFile << filePath << "\n";
        progressFile << offset << "\n";
        progressFile << totalSize << "\n";
    }
}

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

std::string FileSync::getProgressFilePath(const std::string& filePath) {
    return filePath + ".sync_progress";
}

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

std::string FileSync::calculateFileMD5(const std::string& path) {
    return LinuxFileUtil::calculateMD5(path);
}

void FileSync::setSpeedLimit(size_t limit) {
    speedLimit = limit;
}

void FileSync::setConflictStrategy(const std::string& strategy) {
    conflictStrategy = strategy;
}
