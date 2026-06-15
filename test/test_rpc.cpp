#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>
#include <fstream>
#include "rpc/RpcClient.h"

int main() {
    const char* host = "127.0.0.1";
    int port = 29999;
    const char* testDir = "/tmp/rpc_test_dir";

    // 创建测试目录和文件
    mkdir("/tmp/rpc_test_dir", 0755);
    mkdir("/tmp/rpc_test_dir/subdir", 0755);
    std::ofstream f1("/tmp/rpc_test_dir/file1.txt");
    f1 << "hello world" << std::endl;
    f1.close();
    std::ofstream f2("/tmp/rpc_test_dir/subdir/file2.txt");
    f2 << "nested content" << std::endl;
    f2.close();

    std::cout << "=== RPC Client Test ===" << std::endl;

    RpcClient client;
    if (!client.connect(host, port)) {
        std::cerr << "FAIL: connect failed" << std::endl;
        return 1;
    }
    std::cout << "PASS: connected to server" << std::endl;

    // Test 1: listFiles
    {
        synctool::rpc::ListFilesResponse resp;
        if (!client.listFiles("", true, resp)) {
            std::cerr << "FAIL: listFiles failed" << std::endl;
            return 1;
        }
        std::cout << "PASS: listFiles returned " << resp.total_count() << " entries" << std::endl;
        for (int i = 0; i < resp.files().size(); i++) {
            const auto& f = resp.files(i);
            std::cout << "  [" << (f.is_directory() ? "DIR" : "FILE") << "] "
                      << f.path() << " (" << f.size() << " bytes)" << std::endl;
        }
    }

    // Test 2: getFileMeta
    {
        synctool::rpc::GetFileMetaResponse resp;
        if (!client.getFileMeta("file1.txt", resp)) {
            std::cerr << "FAIL: getFileMeta failed" << std::endl;
            return 1;
        }
        std::cout << "PASS: getFileMeta for file1.txt" << std::endl;
        std::cout << "  name: " << resp.meta().name() << std::endl;
        std::cout << "  size: " << resp.meta().size() << std::endl;
        std::cout << "  md5:  " << resp.meta().md5() << std::endl;
        std::cout << "  exists: " << (resp.meta().exists() ? "yes" : "no") << std::endl;
    }

    // Test 3: getFileMeta for non-existent file
    {
        synctool::rpc::GetFileMetaResponse resp;
        if (!client.getFileMeta("nonexistent.txt", resp)) {
            std::cerr << "FAIL: getFileMeta for nonexistent failed" << std::endl;
            return 1;
        }
        std::cout << "PASS: getFileMeta for nonexistent.txt" << std::endl;
        std::cout << "  exists: " << (resp.meta().exists() ? "yes" : "no") << std::endl;
    }

    client.disconnect();
    std::cout << std::endl << "=== All RPC Tests PASSED ===" << std::endl;
    return 0;
}