# modules/preprocess/CMakeLists.txt
cmake_minimum_required(VERSION 3.18)
project(preprocess_module LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 测试开关
option(BUILD_PREPROCESS_TESTS "Build preprocess module tests" OFF)

# 设置库文件输出目录
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)

# 收集源文件
file(GLOB PREPROCESS_SOURCES "src/*.cpp" "src/*/*.cpp")

# 创建静态库
add_library(preprocess_lib STATIC ${PREPROCESS_SOURCES})

# 设置包含目录
target_include_directories(preprocess_lib 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# 链接依赖
target_link_libraries(preprocess_lib
    PUBLIC
        ${OpenCV_LIBS}
        decoder_lib  # preprocess 依赖 decoder 的 VideoFrame
        log_lib
        common_lib        # 通用service接口
)

# Windows 特定配置
if(WIN32)
    target_compile_definitions(preprocess_lib PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

# 如果启用测试，添加测试子目录
if(BUILD_PREPROCESS_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test")
    enable_testing()
    add_subdirectory(test)
endif()
