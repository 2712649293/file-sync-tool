#include "ThreadPool.h"
#include "LogUtil.h"

ThreadPool::ThreadPool(size_t threadCount) : running(true), activeTasks(0) {
    for (size_t i = 0; i < threadCount; i++) {
        threads.emplace_back(&ThreadPool::workerThread, this);
    }
    LogUtil::info("ThreadPool initialized with " + std::to_string(threadCount) + " threads");
}

ThreadPool::~ThreadPool() {
    stop();
}

void ThreadPool::workerThread() {
    while (running) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            condition.wait(lock, [this]() { return !running || !taskQueue.empty(); });
            
            if (!running && taskQueue.empty()) {
                return;
            }
            
            task = std::move(taskQueue.front());
            taskQueue.pop();
        }
        
        try {
            task();
        } catch (const std::exception& e) {
            LogUtil::error("Exception in worker thread: " + std::string(e.what()));
        }
        
        activeTasks--;
    }
}

void ThreadPool::waitForCompletion() {
    while (activeTasks > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

size_t ThreadPool::getThreadCount() const {
    return threads.size();
}

void ThreadPool::setThreadCount(size_t threadCount) {
    if (threadCount == threads.size()) {
        return;
    }
    
    // 停止现有线程
    stop();
    
    // 重新创建线程池
    running = true;
    threads.clear();
    for (size_t i = 0; i < threadCount; i++) {
        threads.emplace_back(&ThreadPool::workerThread, this);
    }
    
    LogUtil::info("ThreadPool resized to " + std::to_string(threadCount) + " threads");
}

void ThreadPool::stop() {
    running = false;
    condition.notify_all();
    
    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    LogUtil::info("ThreadPool stopped");
}

bool ThreadPool::isRunning() const {
    return running;
}
