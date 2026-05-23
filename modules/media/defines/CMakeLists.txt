# modules/media/defines/CMakeLists.txt
# defines 子模块：媒体包/帧的 C++ 内部类及 C API。
cmake_minimum_required(VERSION 3.18)
project(media_defines LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==================== 测试开关 ====================
option(BUILD_MEDIA_TESTS "Build media defines tests" ON)

# ==================== 编译库 ====================
file(GLOB DEFINES_SOURCES "src/*.cpp")
file(GLOB STREAM_SOURCES "../stream/src/*.cpp")

add_library(media_lib STATIC ${DEFINES_SOURCES} ${STREAM_SOURCES})

target_include_directories(media_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../stream/include>
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../puller/include>
        $<INSTALL_INTERFACE:include>
)

# ==================== 查找 FFmpeg ====================
find_path(FFMPEG_INCLUDE_DIR
    NAMES libavcodec/avcodec.h
    PATHS
        "/vcpkg/packages/ffmpeg_x64-windows/include"
        "/vcpkg_installed/x64-windows/include"
        /usr/include
        /usr/local/include
)

find_library(AVCODEC_LIBRARY
    NAMES avcodec
    PATHS
        "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/debug/lib"
        "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/lib"
)

find_library(AVFORMAT_LIBRARY
    NAMES avformat
    PATHS
        "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/debug/lib"
        "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/lib"
)

find_library(AVUTIL_LIBRARY
    NAMES avutil
    PATHS
        "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/debug/lib"
        "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/lib"
)

if(FFMPEG_INCLUDE_DIR)
    target_include_directories(media_lib
        PUBLIC
            ${FFMPEG_INCLUDE_DIR}
    )
endif()

# ==================== 链接依赖 ====================
target_link_libraries(media_lib
    PUBLIC
        common_lib
)

if(AVCODEC_LIBRARY)
    target_link_libraries(media_lib PUBLIC ${AVCODEC_LIBRARY})
endif()

if(AVFORMAT_LIBRARY)
    target_link_libraries(media_lib PUBLIC ${AVFORMAT_LIBRARY})
endif()

if(AVUTIL_LIBRARY)
    target_link_libraries(media_lib PUBLIC ${AVUTIL_LIBRARY})
endif()

# ==================== 平台配置 ====================
if(WIN32)
    target_compile_definitions(media_lib PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

if(MSVC)
    add_compile_options("/utf-8")
endif()

# ==================== 测试子目录 ====================
if(BUILD_MEDIA_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test")
    enable_testing()
    add_subdirectory(test)
endif()