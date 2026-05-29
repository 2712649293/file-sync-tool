#include "LinuxFileUtil.h"
#include <fstream>
#include <dirent.h>
#include <unistd.h>
#include <sys/statvfs.h>
#include <openssl/md5.h>

/**
 * @brief 检查文件是否存在
 * @param path 文件路径
 * @return 文件存在返回true，否则返回false
 */
bool LinuxFileUtil::fileExists(const std::string& path) {
    struct stat statbuf;
    return stat(path.c_str(), &statbuf) == 0;
}

/**
 * @brief 获取文件状态信息
 * @param path 文件路径
 * @param statbuf 状态信息结构体（输出参数）
 * @return 获取成功返回true，否则返回false
 */
bool LinuxFileUtil::getFileStat(const std::string& path, struct stat& statbuf) {
    return stat(path.c_str(), &statbuf) == 0;
}

/**
 * @brief 获取文件大小
 * @param path 文件路径
 * @return 文件大小（字节），失败返回-1
 */
off_t LinuxFileUtil::getFileSize(const std::string& path) {
    struct stat statbuf;
    if (stat(path.c_str(), &statbuf) == 0) {
        return statbuf.st_size;
    }
    return -1;
}

/**
 * @brief 获取文件修改时间
 * @param path 文件路径
 * @return 修改时间戳，失败返回-1
 */
time_t LinuxFileUtil::getFileMtime(const std::string& path) {
    struct stat statbuf;
    if (stat(path.c_str(), &statbuf) == 0) {
        return statbuf.st_mtime;
    }
    return -1;
}

/**
 * @brief 获取文件权限
 * @param path 文件路径
 * @return 文件权限模式，失败返回0
 */
mode_t LinuxFileUtil::getFileMode(const std::string& path) {
    struct stat statbuf;
    if (stat(path.c_str(), &statbuf) == 0) {
        return statbuf.st_mode;
    }
    return 0;
}

/**
 * @brief 设置文件权限
 * @param path 文件路径
 * @param mode 权限模式
 * @return 设置成功返回true，否则返回false
 */
bool LinuxFileUtil::setFileMode(const std::string& path, mode_t mode) {
    return chmod(path.c_str(), mode) == 0;
}

/**
 * @brief 创建目录（支持递归创建）
 * @param path 目录路径
 * @param mode 目录权限（默认0755）
 * @return 创建成功返回true，否则返回false
 */
bool LinuxFileUtil::createDirectory(const std::string& path, mode_t mode) {
    size_t pos = 0;
    size_t next;
    std::string currentPath;
    
    // 递归创建各级目录
    while ((next = path.find('/', pos)) != std::string::npos) {
        currentPath = path.substr(0, next);
        if (!currentPath.empty()) {
            struct stat statbuf;
            if (stat(currentPath.c_str(), &statbuf) != 0) {
                // 目录不存在，创建它
                if (mkdir(currentPath.c_str(), mode) != 0) {
                    return false;
                }
            }
        }
        pos = next + 1;
    }
    
    // 创建最后一级目录
    if (!path.empty()) {
        struct stat statbuf;
        if (stat(path.c_str(), &statbuf) != 0) {
            if (mkdir(path.c_str(), mode) != 0) {
                return false;
            }
        }
    }
    
    return true;
}

/**
 * @brief 列出目录中的所有文件和子目录
 * @param path 目录路径
 * @return 文件和子目录的路径列表
 */
std::vector<std::string> LinuxFileUtil::listDirectory(const std::string& path) {
    std::vector<std::string> files;
    DIR* dir = opendir(path.c_str());
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string name = entry->d_name;
            // 跳过 . 和 ..
            if (name != "." && name != "..") {
                files.push_back(path + "/" + name);
            }
        }
        closedir(dir);
    }
    return files;
}

/**
 * @brief 计算文件的MD5值
 * @param path 文件路径
 * @return MD5哈希值字符串，失败返回空字符串
 */
std::string LinuxFileUtil::calculateMD5(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return "";
    }
    
    // 初始化MD5上下文
    MD5_CTX md5Context;
    MD5_Init(&md5Context);
    
    // 分块读取文件并更新MD5
    char buffer[4096];
    while (file.read(buffer, sizeof(buffer))) {
        MD5_Update(&md5Context, buffer, file.gcount());
    }
    // 处理剩余数据
    if (file.gcount() > 0) {
        MD5_Update(&md5Context, buffer, file.gcount());
    }
    
    // 完成MD5计算
    unsigned char hash[MD5_DIGEST_LENGTH];
    MD5_Final(hash, &md5Context);
    
    // 将二进制哈希值转换为十六进制字符串
    std::string result;
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        char hex[3];
        sprintf(hex, "%02x", hash[i]);
        result += hex;
    }
    
    return result;
}

/**
 * @brief 计算文件部分内容的MD5值
 * @param path 文件路径
 * @param offset 起始偏移量
 * @param size 需要计算的字节数
 * @return MD5哈希值字符串，失败返回空字符串
 */
std::string LinuxFileUtil::calculatePartialMD5(const std::string& path, off_t offset, size_t size) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return "";
    }
    
    // 定位到指定偏移量
    file.seekg(offset);
    if (!file) {
        return "";
    }
    
    // 初始化MD5上下文
    MD5_CTX md5Context;
    MD5_Init(&md5Context);
    
    // 分块读取指定大小的数据并更新MD5
    char buffer[4096];
    size_t remaining = size;
    while (remaining > 0 && file.read(buffer, std::min(sizeof(buffer), remaining))) {
        size_t bytesRead = file.gcount();
        MD5_Update(&md5Context, buffer, bytesRead);
        remaining -= bytesRead;
    }
    
    // 完成MD5计算
    unsigned char hash[MD5_DIGEST_LENGTH];
    MD5_Final(hash, &md5Context);
    
    // 将二进制哈希值转换为十六进制字符串
    std::string result;
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        char hex[3];
        sprintf(hex, "%02x", hash[i]);
        result += hex;
    }
    
    return result;
}

/**
 * @brief 检查磁盘空间是否足够
 * @param path 路径（用于确定文件系统）
 * @param requiredSize 需要的空间大小（字节）
 * @return 空间足够返回true，否则返回false
 */
bool LinuxFileUtil::checkDiskSpace(const std::string& path, off_t requiredSize) {
    struct statvfs statfsbuf;
    if (statvfs(path.c_str(), &statfsbuf) == 0) {
        off_t freeSpace = statfsbuf.f_bavail * statfsbuf.f_frsize;
        return freeSpace >= requiredSize;
    }
    return false;
}

/**
 * @brief 获取磁盘可用空间
 * @param path 路径（用于确定文件系统）
 * @return 可用空间大小（字节），失败返回-1
 */
off_t LinuxFileUtil::getFreeDiskSpace(const std::string& path) {
    struct statvfs statfsbuf;
    if (statvfs(path.c_str(), &statfsbuf) == 0) {
        return statfsbuf.f_bavail * statfsbuf.f_frsize;
    }
    return -1;
}
