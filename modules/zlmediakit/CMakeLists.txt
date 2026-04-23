# modules/zlmediakit/CMakeLists.txt
cmake_minimum_required(VERSION 3.18)
project(zlmediakit_module LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 测试开关：控制是否编译 zlmediakit 模块的测试
option(BUILD_ZLM_TESTS "Build zlmediakit module tests" ON)

# 设置库文件输出目录到 zlmediakit 目录下
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)

# 收集源文件（排除已迁移的 service）
file(GLOB ZLM_SOURCES "src/*.cpp")

# 创建静态库
add_library(zlmediakit_lib STATIC ${ZLM_SOURCES})

# 设置包含目录
target_include_directories(zlmediakit_lib 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# 链接依赖（zlmediakit 只包含封装层，Service 已迁移）
target_link_libraries(zlmediakit_lib
    PUBLIC
        nlohmann_json::nlohmann_json
        Boost::filesystem
        Boost::process
        application_lib   # zlmediakit 模块依赖 application 模块
        common_lib        # IService, log, config
        net_lib           # http_client_lib, io_context_pool_lib
        config_lib        # ZlmConfig
)

# Windows 特定配置
if(WIN32)
    target_compile_definitions(zlmediakit_lib PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
    # 使用 UTF-8 编码，避免中文注释导致的编译错误
    target_compile_options(zlmediakit_lib PRIVATE /utf-8)
endif()

# 定义项目根目录（用于定位 tools 目录）
target_compile_definitions(zlmediakit_lib PRIVATE
    PROJECT_ROOT_DIR="${CMAKE_SOURCE_DIR}"
)

# 如果启用测试，添加测试子目录
if(BUILD_ZLM_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test")
    enable_testing()
    add_subdirectory(test)
endif()
