#include "SyncManager.h"
#include "LogUtil.h"
#include <mutex>
#include <atomic>

/**
 * @brief SyncManager 构造函数
 * @details 初始化同步管理器，创建线程池、异步IO处理器和文件同步器
 */
SyncManager::SyncManager() : 
    threadPool(nullptr), 
    asyncIOHandler(nullptr), 
    fileSync(nullptr), 
    syncing(false), 
    totalTasks(0), 
    completedTasks(0) {
}

/**
 * @brief SyncManager 析构函数
 * @details 停止所有组件并释放资源
 */
SyncManager::~SyncManager() {
    stop();
    delete threadPool;
    delete asyncIOHandler;
    delete fileSync;
}

/**
 * @brief 初始化同步管理器
 * @param threadCount 线程池大小
 */
void SyncManager::init(size_t threadCount) {
    threadPool = new ThreadPool(threadCount);
    asyncIOHandler = new AsyncIOHandler();
    fileSync = new FileSync();
    fileSync->setThreadPool(threadPool);
    fileSync->setAsyncIOHandler(asyncIOHandler);
    asyncIOHandler->start();
    LogUtil::info("SyncManager initialized");
}

/**
 * @brief 执行本地同步
 * @param srcDir 源目录路径
 * @param destDir 目标目录路径
 * @param recursive 是否递归同步（当前未使用，始终递归）
 * @return 同步成功返回true，否则返回false
 */
bool SyncManager::syncLocal(const std::string& srcDir, const std::string& destDir, bool recursive) {
    LogUtil::info("Starting local sync: " + srcDir + " -> " + destDir);
    syncing = true;
    totalTasks = 0;
    completedTasks = 0;
    
    try {
        // 检测变化，生成同步任务
        auto tasks = fileSync->detectChanges(srcDir, destDir);
        totalTasks = tasks.size();
        
        LogUtil::info("Generated " + std::to_string(totalTasks) + " sync tasks");
        
        // 提交任务到线程池并行处理
        for (const auto& task : tasks) {
            threadPool->submit(&SyncManager::processSyncTask, this, task);
        }
        
        // 等待所有任务完成
        threadPool->waitForCompletion();
        
        syncing = false;
        LogUtil::info("Local sync completed successfully");
        return true;
    } catch (const std::exception& e) {
        LogUtil::error("Sync failed: " + std::string(e.what()));
        syncing = false;
        return false;
    }
}

/**
 * @brief 执行远程同步
 * @param srcDir 源目录路径
 * @param destDir 目标目录路径（相对于远程服务器）
 * @param host 远程主机IP地址
 * @param port 远程主机端口
 * @param resume 是否启用断点续传
 * @return 同步成功返回true，否则返回false
 */
bool SyncManager::syncRemote(const std::string& srcDir, const std::string& destDir, const std::string& host, int port, bool resume) {
    LogUtil::info("Starting remote sync: " + srcDir + " -> " + host + ":" + std::to_string(port) + destDir +
                  (resume ? " (resume enabled)" : ""));
    syncing = true;
    totalTasks = 0;
    completedTasks = 0;
    
    try {
        bool result = fileSync->syncRemote(srcDir, destDir, host, port, resume);
        
        syncing = false;
        if (result) {
            LogUtil::info("Remote sync completed successfully");
        } else {
            LogUtil::error("Remote sync failed");
        }
        return result;
    } catch (const std::exception& e) {
        LogUtil::error("Sync failed: " + std::string(e.what()));
        syncing = false;
        return false;
    }
}

/**
 * @brief 从断点续传
 * @param srcPath 源文件路径
 * @param destPath 目标文件路径
 * @return 续传成功返回true，否则返回false
 */
bool SyncManager::resumeSync(const std::string& srcPath, const std::string& destPath) {
    LogUtil::info("Resuming sync: " + srcPath + " -> " + destPath);
    
    try {
        bool result = fileSync->resumeSync(srcPath, destPath);
        if (result) {
            LogUtil::info("Resume sync completed successfully");
        } else {
            LogUtil::error("Resume sync failed");
        }
        return result;
    } catch (const std::exception& e) {
        LogUtil::error("Resume sync failed: " + std::string(e.what()));
        return false;
    }
}

/**
 * @brief 处理单个同步任务
 * @param task 同步任务
 */
void SyncManager::processSyncTask(const SyncTask& task) {
    try {
        fileSync->executeSyncTask(task);
        completedTasks++;
        updateProgress();
    } catch (const std::exception& e) {
        LogUtil::error("Failed to process sync task: " + std::string(e.what()));
    }
}

/**
 * @brief 更新同步进度
 */
void SyncManager::updateProgress() {
    if (totalTasks > 0) {
        double progress = static_cast<double>(completedTasks) / totalTasks * 100;
        LogUtil::info("Sync progress: " + std::to_string(progress) + "%");
    }
}

/**
 * @brief 设置传输速率限制
 * @param limit 速率限制（字节/秒）
 */
void SyncManager::setSpeedLimit(size_t limit) {
    if (fileSync) {
        fileSync->setSpeedLimit(limit);
        LogUtil::info("Set speed limit to " + std::to_string(limit) + " bytes/sec");
    }
}

/**
 * @brief 设置冲突处理策略
 * @param strategy 冲突处理策略（overwrite/skip/rename）
 */
void SyncManager::setConflictStrategy(const std::string& strategy) {
    if (fileSync) {
        fileSync->setConflictStrategy(strategy);
        LogUtil::info("Set conflict strategy to: " + strategy);
    }
}

/**
 * @brief 设置线程池大小
 * @param size 线程池大小
 */
void SyncManager::setThreadPoolSize(size_t size) {
    if (threadPool) {
        threadPool->setThreadCount(size);
    }
}

/**
 * @brief 获取同步进度
 * @return 同步进度百分比（0-100）
 */
double SyncManager::getSyncProgress() const {
    if (totalTasks == 0) {
        return 0.0;
    }
    return static_cast<double>(completedTasks) / totalTasks * 100;
}

/**
 * @brief 停止同步管理器
 */
void SyncManager::stop() {
    syncing = false;
    if (threadPool) {
        threadPool->stop();
    }
    if (asyncIOHandler) {
        asyncIOHandler->stop();
    }
    LogUtil::info("SyncManager stopped");
}

/**
 * @brief 检查是否正在同步
 * @return 正在同步返回true，否则返回false
 */
bool SyncManager::isSyncing() const {
    return syncing;
}
