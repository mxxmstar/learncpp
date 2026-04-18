# modules/grpc/CMakeLists.txt
# GRPC 模块：通用 gRPC 基础设施（客户端/服务器基础类）
# 注意：算法相关的 gRPC 实现（grpc_to_alg, grpc_video_sender）在 alg 模块中

cmake_minimum_required(VERSION 3.18)
project(grpc_module LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 测试开关 - grpc 模块的测试已移到 alg 模块
# option(BUILD_GRPC_TESTS "Build grpc module tests" ON)

# 设置库文件输出目录
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)

# 收集源文件
file(GLOB GRPC_SOURCES "src/*.cpp")

# gRPC 生成的文件（在 modules/grpc/generated 中）
set(GRPC_GENERATED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/generated)
set(GRPC_GENERATED_SOURCES
    ${GRPC_GENERATED_DIR}/hello.pb.cc
    ${GRPC_GENERATED_DIR}/hello.grpc.pb.cc
)

# 创建静态库
add_library(grpc_lib STATIC ${GRPC_SOURCES} ${GRPC_GENERATED_SOURCES})

# 设置包含目录
target_include_directories(grpc_lib 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${GRPC_GENERATED_DIR}>  # gRPC 生成的文件
        $<INSTALL_INTERFACE:include>
)

# 查找 OpenCV（如果尚未查找）- grpc 模块不需要 OpenCV
# if(NOT OpenCV_FOUND)
#     find_package(OpenCV REQUIRED)
# endif()

# 链接依赖
target_link_libraries(grpc_lib
    PUBLIC
        gRPC::grpc++
        protobuf::libprotobuf
        log_lib
        common_lib        # 通用service接口
)

# Windows 特定配置
if(WIN32)
    target_compile_definitions(grpc_lib PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

# grpc 模块的测试已移到 alg 模块
# if(BUILD_GRPC_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test")
#     enable_testing()
#     add_subdirectory(test)
# endif()
