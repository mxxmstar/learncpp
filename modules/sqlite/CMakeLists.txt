# modules/sqlite/CMakeLists.txt
cmake_minimum_required(VERSION 3.18)
project(sqlite_module LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 测试开关：控制是否编译 sqlite 模块的测试
option(BUILD_SQLITE_TESTS "Build sqlite module tests" ON)

# 设置库文件输出目录到 sqlite 目录下
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)

# 收集源文件
file(GLOB SQLITE_SOURCES "src/*.cpp")

# 创建静态库
add_library(sqlite_lib STATIC ${SQLITE_SOURCES})

# 设置包含目录
target_include_directories(sqlite_lib 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Windows 特定配置：链接 SQLite3（使用父作用域中已查找的包）
if(WIN32)
    target_link_libraries(sqlite_lib
        PUBLIC
            unofficial::sqlite3::sqlite3
            log_lib  # sqlite 模块依赖 log 模块
            common_lib        # 通用service接口
    )
else()
    target_link_libraries(sqlite_lib
        PUBLIC
            SQLite::SQLite3
            log_lib  # sqlite 模块依赖 log 模块
            common_lib        # 通用service接口
    )
endif()

# Windows 特定编译定义
if(WIN32)
    target_compile_definitions(sqlite_lib PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
    # 使用 UTF-8 编码，避免中文注释导致的编译错误
    target_compile_options(sqlite_lib PRIVATE /utf-8)
endif()

# 如果启用测试，添加测试子目录
if(BUILD_SQLITE_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test")
    enable_testing()
    add_subdirectory(test)
endif()
