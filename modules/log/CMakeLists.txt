# modules/log/CMakeLists.txt
cmake_minimum_required(VERSION 3.18)
project(log_module LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 测试开关：控制是否编译 log 模块的测试
option(BUILD_LOG_TESTS "Build log module tests" OFF)

# 设置库文件输出目录到 log 目录下
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)

# 收集源文件
file(GLOB LOG_SOURCES "src/*.cpp")

# 创建静态库
add_library(log_lib STATIC ${LOG_SOURCES})

# 设置包含目录
target_include_directories(log_lib 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# 查找依赖
find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)

# 链接依赖
target_link_libraries(log_lib
    PUBLIC
        spdlog::spdlog
        fmt::fmt
)

# Windows 特定配置
if(WIN32)
    target_compile_definitions(log_lib PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX SPDLOG_COMPILED_LIB)
endif()

# 如果启用测试，添加测试子目录
if(BUILD_LOG_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test")
    enable_testing()
    add_subdirectory(test)
endif()
