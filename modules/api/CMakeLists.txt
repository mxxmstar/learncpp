# modules/api/CMakeLists.txt
cmake_minimum_required(VERSION 3.18)
project(api_module LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 测试开关：控制是否编译 api 模块的测试
option(BUILD_API_TESTS "Build api module tests" ON)

# 设置库文件输出目录到 api 目录下
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)

# 收集源文件（只保留 API，Service 已迁移到 service 模块）
file(GLOB API_SOURCES "src/api/*.cpp")
set(API_SOURCES ${API_SOURCES})

# 创建静态库
add_library(api_lib STATIC ${API_SOURCES})

# 设置包含目录
target_include_directories(api_lib 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# 链接依赖（api 模块只包含 API 层）
target_link_libraries(api_lib
    PUBLIC
        nlohmann_json::nlohmann_json
        yaml-cpp::yaml-cpp
        Boost::filesystem
        Boost::process
        application_lib   # api 模块依赖 application 模块
        common_lib        # IService, log, config
        net_lib           # http_server_lib, websocket_lib
        http_server_service_lib   # HttpServerService
        http_client_pool_service_lib  # HttpClientPoolService
        zlmediakit_lib    # ZLMediaKit 封装
        camera_lib        # Camera API
        sqlite_lib        # SQLite（通过 camera_lib 间接依赖）
)

# Windows 特定配置
if(WIN32)
    target_compile_definitions(api_lib PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
    # 使用 UTF-8 编码，避免中文注释导致的编译错误
    target_compile_options(api_lib PRIVATE /utf-8)
    # 链接 Windows 系统库
    target_link_libraries(api_lib
        PUBLIC
            ntdll
            shell32
            advapi32
            user32
            ws2_32
    )
endif()

# 如果启用测试，添加测试子目录
if(BUILD_API_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test")
    enable_testing()
    add_subdirectory(test)
endif()
