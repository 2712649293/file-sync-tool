#ifndef SYNC_MANAGER_H
#define SYNC_MANAGER_H

#include <string>
#include <vector>
#include "ThreadPool.h"
#include "AsyncIOHandler.h"
#include "FileSync.h"

class SyncManager {
public:
    // 构造函数
    SyncManager();
    
    // 析构函数
    ~SyncManager();
    
    // 初始化
    void init(size_t threadCount = 8);
    
    // 同步本地目录
    bool syncLocal(const std::string& srcDir, const std::string& destDir, bool recursive = true);
    
    // 同步远程目录
    bool syncRemote(const std::string& srcDir, const std::string& destDir, const std::string& host, int port, bool resume = false);
    
    // 断点续传
    bool resumeSync(const std::string& srcPath, const std::string& destPath);
    
    // 设置同步速率限制（字节/秒）
    void setSpeedLimit(size_t limit);
    
    // 设置冲突处理策略
    void setConflictStrategy(const std::string& strategy);
    
    // 设置线程池大小
    void setThreadPoolSize(size_t size);
    
    // 获取同步进度
    double getSyncProgress() const;
    
    // 停止同步
    void stop();
    
    // 检查是否正在同步
    bool isSyncing() const;
    
private:
    // 线程池
    ThreadPool* threadPool;
    
    // 异步IO处理器
    AsyncIOHandler* asyncIOHandler;
    
    // 文件同步器
    FileSync* fileSync;
    
    // 同步状态
    bool syncing;
    
    // 同步任务总数
    size_t totalTasks;
    
    // 已完成任务数
    size_t completedTasks;
    
    // 处理同步任务
    void processSyncTask(const SyncTask& task);
    
    // 更新同步进度
    void updateProgress();
};

#endif // SYNC_MANAGER_H
