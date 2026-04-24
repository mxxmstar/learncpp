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

# 添加子模块
add_subdirectory(http_client)
add_subdirectory(http_server)
add_subdirectory(tcp_server)
add_subdirectory(websocket)
add_subdirectory(io_context_pool)

# 创建 INTERFACE 库（只传递依赖，不生成 .lib 文件）
add_library(net_lib INTERFACE)

# 设置包含目录
target_include_directories(net_lib 
    INTERFACE 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# 链接依赖（使用父作用域中已查找的包）
target_link_libraries(net_lib
    INTERFACE
        Boost::asio
        Boost::system
        Boost::json
        spdlog::spdlog
        common_lib  # net 模块依赖 log 模块
        common_lib        # 通用service接口
        http_client_lib   # HTTP 客户端模块
        http_server_lib   # HTTP 服务器模块
        tcp_server_lib    # TCP 服务器模块
        websocket_lib     # WebSocket 模块
        io_context_pool_lib  # IO Context Pool 模块
)

# Windows 特定配置
if(WIN32)
    target_compile_definitions(net_lib INTERFACE WIN32_LEAN_AND_MEAN NOMINMAX SPDLOG_COMPILED_LIB)
endif()

# 如果启用测试，添加测试子目录
if(BUILD_NET_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test")
    enable_testing()
    add_subdirectory(test)
endif()
