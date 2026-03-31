#include "SyncManager.h"
#include "LogUtil.h"
#include <mutex>
#include <atomic>

SyncManager::SyncManager() : 
    threadPool(nullptr), 
    asyncIOHandler(nullptr), 
    fileSync(nullptr), 
    syncing(false), 
    totalTasks(0), 
    completedTasks(0) {
}

SyncManager::~SyncManager() {
    stop();
    delete threadPool;
    delete asyncIOHandler;
    delete fileSync;
}

void SyncManager::init(size_t threadCount) {
    threadPool = new ThreadPool(threadCount);
    asyncIOHandler = new AsyncIOHandler();
    fileSync = new FileSync();
    asyncIOHandler->start();
    LogUtil::info("SyncManager initialized");
}

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
        
        // 提交任务到线程池
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

bool SyncManager::syncRemote(const std::string& srcDir, const std::string& destDir, const std::string& host, int port) {
    LogUtil::info("Starting remote sync: " + srcDir + " -> " + host + ":" + std::to_string(port) + destDir);
    syncing = true;
    totalTasks = 0;
    completedTasks = 0;
    
    try {
        // 使用FileSync的远程同步功能
        bool result = fileSync->syncRemote(srcDir, destDir, host, port);
        
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

void SyncManager::processSyncTask(const SyncTask& task) {
    try {
        fileSync->executeSyncTask(task);
        completedTasks++;
        updateProgress();
    } catch (const std::exception& e) {
        LogUtil::error("Failed to process sync task: " + std::string(e.what()));
    }
}

void SyncManager::updateProgress() {
    if (totalTasks > 0) {
        double progress = static_cast<double>(completedTasks) / totalTasks * 100;
        LogUtil::info("Sync progress: " + std::to_string(progress) + "%");
    }
}

void SyncManager::setSpeedLimit(size_t limit) {
    if (fileSync) {
        fileSync->setSpeedLimit(limit);
        LogUtil::info("Set speed limit to " + std::to_string(limit) + " bytes/sec");
    }
}

void SyncManager::setConflictStrategy(const std::string& strategy) {
    if (fileSync) {
        fileSync->setConflictStrategy(strategy);
        LogUtil::info("Set conflict strategy to: " + strategy);
    }
}

void SyncManager::setThreadPoolSize(size_t size) {
    if (threadPool) {
        threadPool->setThreadCount(size);
    }
}

double SyncManager::getSyncProgress() const {
    if (totalTasks == 0) {
        return 0.0;
    }
    return static_cast<double>(completedTasks) / totalTasks * 100;
}

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

bool SyncManager::isSyncing() const {
    return syncing;
}
