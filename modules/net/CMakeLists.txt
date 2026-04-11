# modules/net/CMakeLists.txt
cmake_minimum_required(VERSION 3.18)
project(net_module LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 测试开关：控制是否编译 net 模块的测试
option(BUILD_NET_TESTS "Build net module tests" OFF)

# 设置库文件输出目录到 net 目录下
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)

# 收集源文件
file(GLOB NET_SOURCES "src/*.cpp")

# 创建静态库
add_library(net_lib STATIC ${NET_SOURCES})

# 设置包含目录
target_include_directories(net_lib 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# 查找依赖
find_package(Boost REQUIRED COMPONENTS asio system json)
find_package(spdlog CONFIG REQUIRED)

# 链接依赖
target_link_libraries(net_lib
    PUBLIC
        Boost::asio
        Boost::system
        Boost::json
        spdlog::spdlog
        log_lib  # net 模块依赖 log 模块
)

# Windows 特定配置
if(WIN32)
    target_compile_definitions(net_lib PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX SPDLOG_COMPILED_LIB)
endif()

# 如果启用测试，添加测试子目录
if(BUILD_NET_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test")
    enable_testing()
    add_subdirectory(test)
endif()
