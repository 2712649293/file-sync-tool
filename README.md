# Product-Grade File Sync Tool

一款基于 Linux 的高性能文件同步工具，对标简化版 rsync，支持增量同步、断点续传，解决大文件、多文件批量同步的效率问题。

## 核心特性

- **基础同步**：支持本地目录双向同步、跨主机（Linux-Linux）单向/双向同步
- **增量同步**：仅同步新增、修改、删除的文件/目录，避免全量同步带来的磁盘 IO、网络带宽浪费
- **断点续传**：同步过程中若出现网络中断、程序异常退出，重启后可从断点处继续同步
- **多线程异步**：采用多线程并发处理多文件同步，结合异步 IO 处理大文件分片传输
- **异常处理**：支持网络中断自动重连、文件权限不足/不存在提示、磁盘空间不足预警
- **易用性**：提供命令行交互（类似 rsync 命令格式），支持 --help 帮助文档、参数配置
- **可部署**：支持 CMake 编译打包，可生成可执行文件，适配 Ubuntu、CentOS 等主流 Linux 发行版

## 系统要求

- Linux 操作系统
- C++11 或更高版本
- OpenSSL 库（用于 MD5 计算）
- CMake 3.10 或更高版本

## 编译安装

### 步骤 1：克隆仓库

```bash
git clone <repository-url>
cd Product-Grade File Sync Tool
```

### 步骤 2：编译

```bash
mkdir -p build
cd build
cmake ..
make
```

### 步骤 3：安装

```bash
sudo make install
```

## 使用方法

### 基本语法

```bash
sync-tool [options] source destination
```

### 选项说明

| 选项        | 长选项           | 描述                            |
| --------- | ------------- | ----------------------------- |
| -s        | --source      | 源目录路径                         |
| -d        | --destination | 目标目录路径                        |
| -h        | --host        | 远程主机 IP 地址                    |
| -p        | --port        | 远程主机端口（默认：8888）               |
| -t        | --threads     | 线程数量（默认：8）                    |
| -l        | --limit       | 同步速率限制（字节/秒）                  |
| -c        | --conflict    | 冲突处理策略（overwrite/skip/rename） |
| -r        | --resume      | 从断点处续传                        |
| --help    | <br />        | 显示帮助信息                        |
| --version | <br />        | 显示版本信息                        |

### 示例

#### 1. 本地同步

```bash
# 同步本地目录
sync-tool -s /path/to/source -d /path/to/destination

# 同步本地目录，使用 16 个线程
sync-tool -s /path/to/source -d /path/to/destination -t 16

# 同步本地目录，限制速率为 10MB/s
sync-tool -s /path/to/source -d /path/to/destination -l 10485760
```

#### 2. 远程同步

```bash
# 同步到远程主机
sync-tool -s /path/to/source -d /path/to/destination -h 192.168.1.100 -p 8888
```

#### 3. 断点续传

```bash
# 从断点处续传文件
sync-tool -s /path/to/source/file -d /path/to/destination/file -r
```

## 项目结构

```
Product-Grade File Sync Tool/
├── CMakeLists.txt          # CMake 配置文件
├── include/                # 头文件目录
│   ├── LinuxFileUtil.h     # Linux 文件操作工具类
│   ├── LogUtil.h           # 日志工具类
│   ├── ExceptionHandler.h  # 异常处理类
│   ├── TcpClient.h         # TCP 客户端类
│   ├── TcpServer.h         # TCP 服务端类
│   ├── FileSync.h          # 文件同步核心类
│   ├── ThreadPool.h        # 线程池类
│   ├── AsyncIOHandler.h    # 异步 IO 处理类
│   └── SyncManager.h       # 同步管理器类
├── src/                    # 源文件目录
│   ├── utils/              # 工具类源文件
│   ├── core/               # 核心功能源文件
│   ├── thread/             # 多线程相关源文件
│   ├── network/            # 网络相关源文件
│   └── cli/                # 命令行接口源文件
├── test/                   # 测试目录
│   └── test_sync.sh        # 测试脚本
└── README.md               # 项目说明文档
```

## 技术实现

### 1. 增量同步

- 通过 `stat/lstat` 系统调用获取文件的 inode、大小、修改时间等元数据
- 结合 MD5 校验（大文件采用分片 MD5）对比文件差异
- 生成增量同步列表，仅同步变化的文件

### 2. 断点续传

- 记录同步进度到 `.sync_progress` 文件
- 重启后读取进度文件，从断点处继续传输
- 支持大文件（10GB+）分片传输（默认分片大小 1MB）

### 3. 多线程异步

- 基于 `std::thread` 创建工作线程，线程数量可配置
- 采用生产者-消费者模型，将同步任务放入任务队列
- 大文件传输采用非阻塞 IO + Epoll 异步事件驱动

### 4. 跨主机同步

- 基于 TCP 长连接实现跨主机通信
- 自定义简单二进制协议，封装数据头部，避免粘包、丢包问题
- 心跳检测机制，自动重连网络中断

## 性能优化

- **多线程并发**：多文件并行同步，提升同步效率
- **异步 IO**：大文件传输采用非阻塞 IO，避免 IO 阻塞
- **元数据缓存**：内存中维护元数据哈希表，减少重复调用 stat/lstat
- **速率限制**：支持同步速率限制，避免占用过多系统资源

## 日志系统

- 分级日志（INFO/WARN/ERROR）
- 支持日志文件输出和控制台输出
- 详细记录同步过程、异常信息，便于排查问题

## 异常处理

- **网络异常**：网络中断自动重连（默认重试 3 次，间隔 5 秒）
- **文件异常**：文件不存在/权限不足提示
- **磁盘异常**：磁盘空间不足预警（剩余空间低于 10%）

## 适用场景

- Linux 服务器日常运维
- 开发环境文件同步
- 轻量级云存储同步
- 大文件跨主机传输

## 许可证

MIT License
