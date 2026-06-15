cmake_minimum_required(VERSION 3.10)
project(sync-tool VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# ── Protobuf ──
set(PROTOBUF_ROOT "/usr/local/protobuf")
set(PROTOBUF_INCLUDE_DIR "${PROTOBUF_ROOT}/include")
set(PROTOBUF_LIB_DIR "${PROTOBUF_ROOT}/lib")

# ── 包含头文件目录 ──
include_directories(include)
include_directories(include/rpc)
include_directories(${PROTOBUF_INCLUDE_DIR})

# 收集源文件
file(GLOB_RECURSE SOURCES "src/**/*.cpp")

# 添加 proto 生成的源文件
list(APPEND SOURCES "src/rpc/sync_service.pb.cc")

# 添加可执行文件
add_executable(sync-tool ${SOURCES})

# ── 链接依赖库 ──
find_package(OpenSSL REQUIRED)
find_library(PROTOBUF_LIB protobuf PATHS ${PROTOBUF_LIB_DIR} REQUIRED)

target_link_libraries(sync-tool
    OpenSSL::SSL
    OpenSSL::Crypto
    ${PROTOBUF_LIB}
)

target_link_directories(sync-tool PRIVATE ${PROTOBUF_LIB_DIR})

# 设置编译选项
target_compile_options(sync-tool PRIVATE -Wall -Wextra -pedantic)

# 安装配置
install(TARGETS sync-tool DESTINATION bin)

# 打包配置
set(CPACK_PACKAGE_NAME "sync-tool")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Linux high-performance file sync tool")
set(CPACK_PACKAGE_CONTACT "example@example.com")
set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Maintainer")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libssl-dev")

include(CPack)