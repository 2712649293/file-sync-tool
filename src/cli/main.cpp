#include <iostream>
#include <getopt.h>
#include <cstring>
#include "SyncManager.h"
#include "LogUtil.h"

void printHelp() {
    std::cout << "Usage: sync-tool [options] source destination" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -s, --source       Source directory" << std::endl;
    std::cout << "  -d, --destination  Destination directory" << std::endl;
    std::cout << "  -h, --host         Remote host IP" << std::endl;
    std::cout << "  -p, --port         Remote host port" << std::endl;
    std::cout << "  -t, --threads      Number of threads (default: 8)" << std::endl;
    std::cout << "  -l, --limit        Speed limit (bytes/sec)" << std::endl;
    std::cout << "  -c, --conflict     Conflict strategy (overwrite/skip/rename)" << std::endl;
    std::cout << "  -r, --resume       Resume sync from breakpoint" << std::endl;
    std::cout << "  --help             Show this help message" << std::endl;
    std::cout << "  --version          Show version information" << std::endl;
}

void printVersion() {
    std::cout << "sync-tool version 1.0.0" << std::endl;
}

int main(int argc, char* argv[]) {
    // 初始化日志
    LogUtil::init("sync-tool.log");
    
    // 默认参数
    std::string source;
    std::string destination;
    std::string host;
    int port = 8888;
    size_t threads = 8;
    size_t speedLimit = 0;
    std::string conflictStrategy = "overwrite";
    bool resume = false;
    bool remoteSync = false;
    
    // 解析命令行参数
    static struct option long_options[] = {
        {"source", required_argument, 0, 's'},
        {"destination", required_argument, 0, 'd'},
        {"host", required_argument, 0, 'h'},
        {"port", required_argument, 0, 'p'},
        {"threads", required_argument, 0, 't'},
        {"limit", required_argument, 0, 'l'},
        {"conflict", required_argument, 0, 'c'},
        {"resume", no_argument, 0, 'r'},
        {"help", no_argument, 0, 0},
        {"version", no_argument, 0, 0},
        {0, 0, 0, 0}
    };
    
    int option_index = 0;
    int c;
    
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
                remoteSync = true;
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
            case 0:
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
    
    // 检查必要参数
    if (source.empty() || destination.empty()) {
        std::cerr << "Error: Source and destination directories are required" << std::endl;
        printHelp();
        return 1;
    }
    
    // 初始化SyncManager
    SyncManager syncManager;
    syncManager.init(threads);
    
    // 设置参数
    if (speedLimit > 0) {
        syncManager.setSpeedLimit(speedLimit);
    }
    syncManager.setConflictStrategy(conflictStrategy);
    
    // 执行同步
    bool success = false;
    if (resume) {
        success = syncManager.resumeSync(source, destination);
    } else if (remoteSync) {
        success = syncManager.syncRemote(source, destination, host, port);
    } else {
        success = syncManager.syncLocal(source, destination);
    }
    
    // 清理
    syncManager.stop();
    LogUtil::close();
    
    return success ? 0 : 1;
}
