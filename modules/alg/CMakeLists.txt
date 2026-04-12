# modules/alg/CMakeLists.txt
cmake_minimum_required(VERSION 3.18)
project(alg_module LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 测试开关
option(BUILD_ALG_TESTS "Build alg module tests" ON)

# 设置库文件输出目录
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)

# 收集源文件（包括 grpc 子目录）
file(GLOB ALG_SOURCES "src/*.cpp" "src/*/*.cpp")

# gRPC 生成的文件（video_processing.proto - 算法相关）
set(ALG_GRPC_GENERATED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/generated)
set(ALG_GRPC_GENERATED_SOURCES
    ${ALG_GRPC_GENERATED_DIR}/video_processing.pb.cc
    ${ALG_GRPC_GENERATED_DIR}/video_processing.grpc.pb.cc
)

# 创建静态库
add_library(alg_lib STATIC ${ALG_SOURCES} ${ALG_GRPC_GENERATED_SOURCES})

# 设置包含目录
target_include_directories(alg_lib 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${ALG_GRPC_GENERATED_DIR}>  # gRPC 生成的文件
        $<INSTALL_INTERFACE:include>
)

# 链接依赖
target_link_libraries(alg_lib
    PUBLIC
        ${OpenCV_LIBS}
        log_lib
        grpc_lib  # alg 依赖 grpc 模块的通用基础设施
)

# Windows 特定配置
if(WIN32)
    target_compile_definitions(alg_lib PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

# 如果启用测试，添加测试子目录
if(BUILD_ALG_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test")
    enable_testing()
    add_subdirectory(test)
endif()
