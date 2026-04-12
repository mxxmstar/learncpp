# modules/puller/CMakeLists.txt
cmake_minimum_required(VERSION 3.18)
project(puller_module LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 测试开关：控制是否编译 puller 模块的测试
option(BUILD_PULLER_TESTS "Build puller module tests" ON)

# 设置库文件输出目录到 puller 目录下
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)

# 收集源文件
file(GLOB PULLER_SOURCES "src/*.cpp" "src/*/*.cpp")

# 创建静态库
add_library(puller_lib STATIC ${PULLER_SOURCES})

# 设置包含目录
target_include_directories(puller_lib 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# 链接依赖（使用父作用域中已查找的包）
target_link_libraries(puller_lib
    PUBLIC
        Boost::asio
        Boost::beast
        log_lib  # puller 模块依赖 log 模块
)

# Windows 特定配置
if(WIN32)
    target_compile_definitions(puller_lib PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0601)
endif()

# 如果启用测试，添加测试子目录
if(BUILD_PULLER_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test")
    enable_testing()
    add_subdirectory(test)
endif()
