#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

class ThreadPool {
public:
    // 构造函数
    ThreadPool(size_t threadCount = 8);
    
    // 析构函数
    ~ThreadPool();
    
    // 提交任务
    template<typename F, typename... Args>
    void submit(F&& f, Args&&... args);
    
    // 等待所有任务完成
    void waitForCompletion();
    
    // 获取线程数量
    size_t getThreadCount() const;
    
    // 设置线程数量
    void setThreadCount(size_t threadCount);
    
    // 停止线程池
    void stop();
    
    // 检查线程池是否运行
    bool isRunning() const;
    
private:
    // 工作线程函数
    void workerThread();
    
    // 线程池
    std::vector<std::thread> threads;
    
    // 任务队列
    std::queue<std::function<void()>> taskQueue;
    
    // 同步原语
    std::mutex queueMutex;
    std::condition_variable condition;
    
    // 状态标志
    std::atomic<bool> running;
    std::atomic<size_t> activeTasks;
};

// 模板方法实现
template<typename F, typename... Args>
void ThreadPool::submit(F&& f, Args&&... args) {
    {   
        std::lock_guard<std::mutex> lock(queueMutex);
        taskQueue.emplace([f = std::forward<F>(f), args = std::make_tuple(std::forward<Args>(args)...)]() {
            std::apply(f, args);
        });
        activeTasks++;
    }
    condition.notify_one();
}

#endif // THREAD_POOL_H
