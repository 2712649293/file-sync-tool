#ifndef FILE_SYNC_H
#define FILE_SYNC_H

#include <string>
#include <vector>
#include <unordered_map>
#include <sys/stat.h>

struct FileMetadata {
    std::string path;
    off_t size;
    time_t mtime;
    mode_t mode;
    std::string md5;
};

struct SyncTask {
    std::string srcPath;
    std::string destPath;
    bool isDirectory;
    mode_t mode;
};

struct SyncProgress {
    std::string filePath;
    off_t offset;
    off_t totalSize;
};

class FileSync {
public:
    // 构造函数
    FileSync();
    
    // 析构函数
    ~FileSync();
    
    // 本地同步
    bool syncLocal(const std::string& srcDir, const std::string& destDir, bool recursive = true);
    
    // 跨主机同步
    bool syncRemote(const std::string& srcDir, const std::string& destDir, const std::string& host, int port);
    
    // 增量同步检测
    std::vector<SyncTask> detectChanges(const std::string& srcDir, const std::string& destDir);
    
    // 执行同步任务
    bool executeSyncTask(const SyncTask& task);
    
    // 断点续传
    bool resumeSync(const std::string& srcPath, const std::string& destPath);
    
    // 保存同步进度
    void saveProgress(const std::string& filePath, off_t offset, off_t totalSize);
    
    // 加载同步进度
    SyncProgress loadProgress(const std::string& filePath);
    
    // 设置同步速率限制（字节/秒）
    void setSpeedLimit(size_t limit);
    
    // 设置冲突处理策略
    void setConflictStrategy(const std::string& strategy);
    
private:
    // 采集目录元数据
    std::unordered_map<std::string, FileMetadata> collectMetadata(const std::string& directory);
    
    // 比较元数据，生成同步任务
    std::vector<SyncTask> compareMetadata(
        const std::unordered_map<std::string, FileMetadata>& srcMetadata,
        const std::unordered_map<std::string, FileMetadata>& destMetadata,
        const std::string& srcDir,
        const std::string& destDir
    );
    
    // 同步文件
    bool syncFile(const std::string& srcPath, const std::string& destPath, mode_t mode);
    
    // 同步目录
    bool syncDirectory(const std::string& srcPath, const std::string& destPath, mode_t mode);
    
    // 传输文件（支持断点续传）
    bool transferFile(const std::string& srcPath, const std::string& destPath, off_t offset = 0);
    
    // 计算文件MD5
    std::string calculateFileMD5(const std::string& path);
    
    // 检查文件是否需要同步
    bool needSync(const FileMetadata& srcMeta, const FileMetadata& destMeta);
    
    // 处理文件冲突
    bool handleConflict(const std::string& srcPath, const std::string& destPath);
    
    // 进度文件路径
    std::string getProgressFilePath(const std::string& filePath);
    
    // 同步速率限制
    size_t speedLimit;
    
    // 冲突处理策略
    std::string conflictStrategy;
};

#endif // FILE_SYNC_H
