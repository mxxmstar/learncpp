# modules/decoder/CMakeLists.txt
cmake_minimum_required(VERSION 3.18)
project(decoder_module LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 测试开关
option(BUILD_DECODER_TESTS "Build decoder module tests" ON)

# 设置库文件输出目录
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)

# 收集源文件
file(GLOB DECODER_SOURCES "src/*.cpp" "src/*/*.cpp")

# 创建静态库
add_library(decoder_lib STATIC ${DECODER_SOURCES})

# 设置包含目录
target_include_directories(decoder_lib 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# 链接依赖
target_link_libraries(decoder_lib
    PUBLIC
        ${FFMPEG_LIBRARIES}
        log_lib
)

# Windows 特定配置
if(WIN32)
    target_compile_definitions(decoder_lib PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX __STDC_CONSTANT_MACROS __STDC_LIMIT_MACROS)
endif()

# 如果启用测试，添加测试子目录
if(BUILD_DECODER_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test")
    enable_testing()
    add_subdirectory(test)
endif()
