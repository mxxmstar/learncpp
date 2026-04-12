# modules/service/CMakeLists.txt
cmake_minimum_required(VERSION 3.18)
project(service_module LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 测试开关：控制是否编译 service 模块的测试
option(BUILD_SERVICE_TESTS "Build service module tests" OFF)

# 设置库文件输出目录到 service 目录下
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)

# 收集源文件
file(GLOB SERVICE_SOURCES "src/*.cpp")

# 创建静态库
add_library(service_lib STATIC ${SERVICE_SOURCES})

# 设置包含目录
target_include_directories(service_lib 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# 链接依赖（使用父作用域中已查找的包）
target_link_libraries(service_lib
    PUBLIC
        yaml-cpp::yaml-cpp
        log_lib  # service 模块依赖 log 模块
        net_lib  # service 模块依赖 net 模块
        zlmediakit_lib  # service 模块依赖 zlmediakit 模块
        config_lib  # service 模块依赖 config 模块
)

# Windows 特定配置
if(WIN32)
    target_compile_definitions(service_lib PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
    # 使用 UTF-8 编码，避免中文注释导致的编译错误
    target_compile_options(service_lib PRIVATE /utf-8)
endif()

# 如果启用测试，添加测试子目录
if(BUILD_SERVICE_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test")
    enable_testing()
    add_subdirectory(test)
endif()
