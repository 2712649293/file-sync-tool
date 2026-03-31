#include "LinuxFileUtil.h"
#include <fstream>
#include <dirent.h>
#include <unistd.h>
#include <sys/statvfs.h>
#include <openssl/md5.h>

bool LinuxFileUtil::fileExists(const std::string& path) {
    struct stat statbuf;
    return stat(path.c_str(), &statbuf) == 0;
}

bool LinuxFileUtil::getFileStat(const std::string& path, struct stat& statbuf) {
    return stat(path.c_str(), &statbuf) == 0;
}

off_t LinuxFileUtil::getFileSize(const std::string& path) {
    struct stat statbuf;
    if (stat(path.c_str(), &statbuf) == 0) {
        return statbuf.st_size;
    }
    return -1;
}

time_t LinuxFileUtil::getFileMtime(const std::string& path) {
    struct stat statbuf;
    if (stat(path.c_str(), &statbuf) == 0) {
        return statbuf.st_mtime;
    }
    return -1;
}

mode_t LinuxFileUtil::getFileMode(const std::string& path) {
    struct stat statbuf;
    if (stat(path.c_str(), &statbuf) == 0) {
        return statbuf.st_mode;
    }
    return 0;
}

bool LinuxFileUtil::setFileMode(const std::string& path, mode_t mode) {
    return chmod(path.c_str(), mode) == 0;
}

bool LinuxFileUtil::createDirectory(const std::string& path, mode_t mode) {
    size_t pos = 0;
    size_t next;
    std::string currentPath;
    
    while ((next = path.find('/', pos)) != std::string::npos) {
        currentPath = path.substr(0, next);
        if (!currentPath.empty()) {
            struct stat statbuf;
            if (stat(currentPath.c_str(), &statbuf) != 0) {
                if (mkdir(currentPath.c_str(), mode) != 0) {
                    return false;
                }
            }
        }
        pos = next + 1;
    }
    
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

std::vector<std::string> LinuxFileUtil::listDirectory(const std::string& path) {
    std::vector<std::string> files;
    DIR* dir = opendir(path.c_str());
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string name = entry->d_name;
            if (name != "." && name != "..") {
                files.push_back(path + "/" + name);
            }
        }
        closedir(dir);
    }
    return files;
}

std::string LinuxFileUtil::calculateMD5(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return "";
    }
    
    MD5_CTX md5Context;
    MD5_Init(&md5Context);
    
    char buffer[4096];
    while (file.read(buffer, sizeof(buffer))) {
        MD5_Update(&md5Context, buffer, file.gcount());
    }
    if (file.gcount() > 0) {
        MD5_Update(&md5Context, buffer, file.gcount());
    }
    
    unsigned char hash[MD5_DIGEST_LENGTH];
    MD5_Final(hash, &md5Context);
    
    std::string result;
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        char hex[3];
        sprintf(hex, "%02x", hash[i]);
        result += hex;
    }
    
    return result;
}

std::string LinuxFileUtil::calculatePartialMD5(const std::string& path, off_t offset, size_t size) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return "";
    }
    
    file.seekg(offset);
    if (!file) {
        return "";
    }
    
    MD5_CTX md5Context;
    MD5_Init(&md5Context);
    
    char buffer[4096];
    size_t remaining = size;
    while (remaining > 0 && file.read(buffer, std::min(sizeof(buffer), remaining))) {
        size_t bytesRead = file.gcount();
        MD5_Update(&md5Context, buffer, bytesRead);
        remaining -= bytesRead;
    }
    
    unsigned char hash[MD5_DIGEST_LENGTH];
    MD5_Final(hash, &md5Context);
    
    std::string result;
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        char hex[3];
        sprintf(hex, "%02x", hash[i]);
        result += hex;
    }
    
    return result;
}

bool LinuxFileUtil::checkDiskSpace(const std::string& path, off_t requiredSize) {
    struct statvfs statfsbuf;
    if (statvfs(path.c_str(), &statfsbuf) == 0) {
        off_t freeSpace = statfsbuf.f_bavail * statfsbuf.f_frsize;
        return freeSpace >= requiredSize;
    }
    return false;
}

off_t LinuxFileUtil::getFreeDiskSpace(const std::string& path) {
    struct statvfs statfsbuf;
    if (statvfs(path.c_str(), &statfsbuf) == 0) {
        return statfsbuf.f_bavail * statfsbuf.f_frsize;
    }
    return -1;
}
