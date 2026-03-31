#!/bin/bash

# 测试脚本：测试sync-tool的基本功能

echo "=== Testing sync-tool ==="

# 创建测试目录
mkdir -p test_src test_dest

# 创建测试文件
echo "test content 1" > test_src/file1.txt
echo "test content 2" > test_src/file2.txt
mkdir -p test_src/subdir
echo "test content 3" > test_src/subdir/file3.txt

# 测试本地同步
echo "\n1. Testing local sync..."
../build/sync-tool -s test_src -d test_dest

# 检查同步结果
echo "\nChecking sync results..."
ls -la test_dest
cat test_dest/file1.txt
cat test_dest/file2.txt
cat test_dest/subdir/file3.txt

# 测试增量同步
echo "\n2. Testing incremental sync..."
echo "updated content" > test_src/file1.txt
../build/sync-tool -s test_src -d test_dest
cat test_dest/file1.txt

# 测试断点续传模拟
echo "\n3. Testing resume sync..."
# 创建大文件
dd if=/dev/zero of=test_src/large_file.bin bs=1M count=10
# 创建目标目录
mkdir -p test_dest
# 创建正确格式的进度文件
echo "test_dest/large_file.bin" > test_dest/large_file.bin.sync_progress
echo "5242880" >> test_dest/large_file.bin.sync_progress  # 5MB 偏移量
echo "10485760" >> test_dest/large_file.bin.sync_progress  # 10MB 总大小
# 执行断点续传
../build/sync-tool -s test_src/large_file.bin -d test_dest/large_file.bin -r

# 清理测试文件
echo "\n4. Cleaning up..."
rm -rf test_src test_dest sync-tool.log

echo "\n=== Test completed ==="
