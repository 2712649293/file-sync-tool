#include "ThreadPool.h"
#include "LogUtil.h"

/**
 * @brief ThreadPool 构造函数
 * @details 创建指定数量的工作线程
 * @param threadCount 线程数量
 */
ThreadPool::ThreadPool(size_t threadCount) : running(true), activeTasks(0) {
    for (size_t i = 0; i < threadCount; i++) {
        threads.emplace_back(&ThreadPool::workerThread, this);
    }
    LogUtil::info("ThreadPool initialized with " + std::to_string(threadCount) + " threads");
}

/**
 * @brief ThreadPool 析构函数
 * @details 停止所有线程并释放资源
 */
ThreadPool::~ThreadPool() {
    stop();
}

/**
 * @brief 工作线程主循环
 * @details 循环从任务队列中获取任务并执行
 */
void ThreadPool::workerThread() {
    while (running) {
        std::function<void()> task;
        {
            // 加锁获取任务
            std::unique_lock<std::mutex> lock(queueMutex);
            // 等待条件：线程池停止 或 任务队列非空
            condition.wait(lock, [this]() { return !running || !taskQueue.empty(); });
            
            // 如果线程池已停止且任务队列为空，退出
            if (!running && taskQueue.empty()) {
                return;
            }
            
            // 获取任务
            task = std::move(taskQueue.front());
            taskQueue.pop();
        }
        
        // 执行任务（已解锁）
        try {
            task();
        } catch (const std::exception& e) {
            LogUtil::error("Exception in worker thread: " + std::string(e.what()));
        }
        
        activeTasks--;
    }
}

/**
 * @brief 等待所有任务完成
 */
void ThreadPool::waitForCompletion() {
    while (activeTasks > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

/**
 * @brief 获取线程池大小
 * @return 线程数量
 */
size_t ThreadPool::getThreadCount() const {
    return threads.size();
}

/**
 * @brief 设置线程池大小
 * @param threadCount 新的线程数量
 */
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

/**
 * @brief 停止线程池
 * @details 停止所有工作线程
 */
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

/**
 * @brief 检查线程池是否正在运行
 * @return 正在运行返回true，否则返回false
 */
bool ThreadPool::isRunning() const {
    return running;
}
