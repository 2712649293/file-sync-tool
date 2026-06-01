#include <iostream>
#include <getopt.h>
#include <cstring>
#include <csignal>
#include <thread>
#include "SyncManager.h"
#include "SyncServer.h"
#include "LogUtil.h"

// 服务器运行状态标志（volatile确保多线程可见性）
static volatile bool serverRunning = true;

/**
 * @brief 信号处理函数
 * @details 处理SIGINT和SIGTERM信号，用于优雅地停止服务器
 * @param sig 信号编号
 */
static void signalHandler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        serverRunning = false;
    }
}

/**
 * @brief 打印帮助信息
 * @details 输出命令行工具的使用说明和选项列表
 */
void printHelp() {
    std::cout << "Usage: sync-tool [options] source destination" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -s, --source       Source directory" << std::endl;
    std::cout << "  -d, --destination  Destination directory" << std::endl;
    std::cout << "  -h, --host         Remote host IP" << std::endl;
    std::cout << "  -p, --port         Port number (default: 8888)" << std::endl;
    std::cout << "  -t, --threads      Number of threads (default: 8)" << std::endl;
    std::cout << "  -l, --limit        Speed limit (bytes/sec)" << std::endl;
    std::cout << "  -c, --conflict     Conflict strategy (overwrite/skip/rename)" << std::endl;
    std::cout << "  -r, --resume       Resume sync from breakpoint" << std::endl;
    std::cout << "  --server           Start in server mode" << std::endl;
    std::cout << "  --server-dir DIR   Server base directory for received files" << std::endl;
    std::cout << "  --help             Show this help message" << std::endl;
    std::cout << "  --version          Show version information" << std::endl;
}

/**
 * @brief 打印版本信息
 */
void printVersion() {
    std::cout << "sync-tool version 1.0.0" << std::endl;
}

/**
 * @brief 主函数
 * @details 文件同步工具的入口函数，支持客户端模式和服务器模式
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * @return 成功返回0，失败返回1
 */
int main(int argc, char* argv[]) {
    // 初始化日志工具
    LogUtil::init("sync-tool.log");

    // 命令行参数解析结果
    std::string source;              // 源目录路径
    std::string destination;         // 目标目录路径
    std::string host;               // 远程主机IP
    int port = 8888;                 // 端口号（默认8888）
    size_t threads = 8;              // 线程池大小（默认8）
    size_t speedLimit = 0;           // 速率限制（字节/秒，0表示无限制）
    std::string conflictStrategy = "overwrite";  // 冲突处理策略
    bool resume = false;             // 是否断点续传
    bool remoteSync = false;         // 是否远程同步
    bool serverMode = false;         // 是否服务器模式
    std::string serverDir;           // 服务器基础目录

    // 长选项定义
    static struct option long_options[] = {
        {"source", required_argument, 0, 's'},
        {"destination", required_argument, 0, 'd'},
        {"host", required_argument, 0, 'h'},
        {"port", required_argument, 0, 'p'},
        {"threads", required_argument, 0, 't'},
        {"limit", required_argument, 0, 'l'},
        {"conflict", required_argument, 0, 'c'},
        {"resume", no_argument, 0, 'r'},
        {"server", no_argument, 0, 1000},
        {"server-dir", required_argument, 0, 1001},
        {"help", no_argument, 0, 0},
        {"version", no_argument, 0, 0},
        {0, 0, 0, 0}
    };

    int option_index = 0;
    int c;

    // 解析命令行参数
    while ((c = getopt_long(argc, argv, "s:d:h:p:t:l:c:r", long_options, &option_index)) != -1) {
        switch (c) {
            case 's':
                source = optarg;
                break;
            case 'd':
                destination = optarg;
                break;
            case 'h':
                host = optarg;
                remoteSync = true;  // 指定host意味着远程同步
                break;
            case 'p':
                port = std::stoi(optarg);
                break;
            case 't':
                threads = std::stoul(optarg);
                break;
            case 'l':
                speedLimit = std::stoul(optarg);
                break;
            case 'c':
                conflictStrategy = optarg;
                break;
            case 'r':
                resume = true;
                break;
            case 1000:
                serverMode = true;
                break;
            case 1001:
                serverDir = optarg;
                break;
            case 0:
                // 处理不带短选项的长选项
                if (strcmp(long_options[option_index].name, "help") == 0) {
                    printHelp();
                    return 0;
                } else if (strcmp(long_options[option_index].name, "version") == 0) {
                    printVersion();
                    return 0;
                }
                break;
            default:
                printHelp();
                return 1;
        }
    }

    // 服务器模式
    if (serverMode) {
        // 注册信号处理函数
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);

        // 创建并启动同步服务器
        SyncServer syncServer;
        if (!serverDir.empty()) {
            syncServer.setBaseDirectory(serverDir);
        }
        if (!syncServer.start(port, threads)) {
            LogUtil::error("Failed to start SyncServer");
            LogUtil::close();
            return 1;
        }

        // 主循环等待停止信号
        LogUtil::info("SyncServer is running on port " + std::to_string(port) + ". Press Ctrl+C to stop.");
        while (serverRunning) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        // 停止服务器并清理资源
        syncServer.stop();
        LogUtil::close();
        return 0;
    }

    // 客户端模式：验证必要参数
    if (source.empty() || destination.empty()) {
        std::cerr << "Error: Source and destination directories are required" << std::endl;
        printHelp();
        return 1;
    }

    // 创建并初始化同步管理器
    SyncManager syncManager;
    syncManager.init(threads);

    // 设置速率限制
    if (speedLimit > 0) {
        syncManager.setSpeedLimit(speedLimit);
    }
    // 设置冲突处理策略
    syncManager.setConflictStrategy(conflictStrategy);

    // 执行同步操作
    bool success = false;
    if (resume && remoteSync) {
        success = syncManager.syncRemote(source, destination, host, port, true);
    } else if (resume) {
        success = syncManager.resumeSync(source, destination);
    } else if (remoteSync) {
        success = syncManager.syncRemote(source, destination, host, port);
    } else {
        success = syncManager.syncLocal(source, destination);
    }

    // 停止同步管理器并关闭日志
    syncManager.stop();
    LogUtil::close();

    // 返回执行结果
    return success ? 0 : 1;
}
