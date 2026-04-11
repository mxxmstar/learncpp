# modules/config/CMakeLists.txt
cmake_minimum_required(VERSION 3.18)
project(config_module LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 测试开关：控制是否编译 config 模块的测试
option(BUILD_CONFIG_TESTS "Build config module tests" OFF)

# 设置库文件输出目录到 config 目录下
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)

# 收集源文件
file(GLOB CONFIG_SOURCES "src/*.cpp")

# 创建静态库
add_library(config_lib STATIC ${CONFIG_SOURCES})

# 设置包含目录
target_include_directories(config_lib 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# 查找依赖
find_package(yaml-cpp CONFIG REQUIRED)

# 链接依赖
target_link_libraries(config_lib
    PUBLIC
        yaml-cpp::yaml-cpp
        log_lib  # config 模块依赖 log 模块
        net_lib  # config 模块依赖 net 模块
)

# Windows 特定配置
if(WIN32)
    target_compile_definitions(config_lib PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
    # 使用 UTF-8 编码，避免中文注释导致的编译错误
    target_compile_options(config_lib PRIVATE /utf-8)
endif()

# 如果启用测试，添加测试子目录
if(BUILD_CONFIG_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test")
    enable_testing()
    add_subdirectory(test)
endif()
