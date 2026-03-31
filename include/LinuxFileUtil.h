#ifndef LINUX_FILE_UTIL_H
#define LINUX_FILE_UTIL_H

#include <string>
#include <vector>
#include <sys/stat.h>

class LinuxFileUtil {
public:
    // 检查文件是否存在
    static bool fileExists(const std::string& path);
    
    // 获取文件状态
    static bool getFileStat(const std::string& path, struct stat& statbuf);
    
    // 获取文件大小
    static off_t getFileSize(const std::string& path);
    
    // 获取文件修改时间
    static time_t getFileMtime(const std::string& path);
    
    // 获取文件权限
    static mode_t getFileMode(const std::string& path);
    
    // 设置文件权限
    static bool setFileMode(const std::string& path, mode_t mode);
    
    // 创建目录（递归）
    static bool createDirectory(const std::string& path, mode_t mode = 0755);
    
    // 列出目录下的所有文件和子目录
    static std::vector<std::string> listDirectory(const std::string& path);
    
    // 计算文件的MD5值
    static std::string calculateMD5(const std::string& path);
    
    // 计算文件分片的MD5值
    static std::string calculatePartialMD5(const std::string& path, off_t offset, size_t size);
    
    // 检查磁盘空间
    static bool checkDiskSpace(const std::string& path, off_t requiredSize);
    
    // 获取剩余磁盘空间
    static off_t getFreeDiskSpace(const std::string& path);
};

#endif // LINUX_FILE_UTIL_H
