# modules/web/CMakeLists.txt
cmake_minimum_required(VERSION 3.18)
project(web_module LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 测试开关：控制是否编译 web 模块的测试
option(BUILD_WEB_TESTS "Build web module tests" ON)

# 设置库文件输出目录到 web 目录下
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)

# 收集源文件
file(GLOB WEB_API_SOURCES "src/api/*.cpp")
file(GLOB WEB_SERVICE_SOURCES "src/service/*.cpp")
set(WEB_SOURCES ${WEB_API_SOURCES} ${WEB_SERVICE_SOURCES})

# 创建静态库
add_library(web_lib STATIC ${WEB_SOURCES})

# 设置包含目录
target_include_directories(web_lib 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# 链接依赖（合并 api 和 service 的依赖）
target_link_libraries(web_lib
    PUBLIC
        nlohmann_json::nlohmann_json
        yaml-cpp::yaml-cpp
        Boost::filesystem
        Boost::process
        application_lib   # web 模块依赖 application 模块（IService, ServiceContainer）
        log_lib           # web 模块依赖 log 模块
        net_lib           # web 模块依赖 net 模块
        http_client_lib   # HTTP 客户端模块
        http_server_lib   # HTTP 服务器模块
        websocket_lib     # WebSocket 模块
        io_context_pool_lib  # IO Context Pool
        zlmediakit_lib    # web 模块依赖 zlmediakit 模块
        config_lib        # web 模块依赖 config 模块
        camera_lib        # web 模块依赖 camera 模块（Camera API）
        sqlite_lib        # web 模块依赖 sqlite 模块（通过 camera_lib 间接依赖）
        common_lib        # 通用service接口
)

# Windows 特定配置
if(WIN32)
    target_compile_definitions(web_lib PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
    # 使用 UTF-8 编码，避免中文注释导致的编译错误
    target_compile_options(web_lib PRIVATE /utf-8)
    # 链接 Windows 系统库
    target_link_libraries(web_lib
        PUBLIC
            ntdll
            shell32
            advapi32
            user32
            ws2_32
    )
endif()

# 如果启用测试，添加测试子目录
if(BUILD_WEB_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test")
    enable_testing()
    add_subdirectory(test)
endif()
