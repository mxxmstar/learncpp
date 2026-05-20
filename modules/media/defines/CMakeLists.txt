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

add_library(media_lib STATIC )

target_include_directories(media_lib
    PUBLIC
        $<BUILD_INTERFACE:/include>
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
        "/vcpkg/packages/ffmpeg_x64-windows/lib"
        "/vcpkg_installed/x64-windows/debug/lib"
        "/vcpkg_installed/x64-windows/lib"
)

find_library(AVUTIL_LIBRARY
    NAMES avutil
    PATHS
        "/vcpkg/packages/ffmpeg_x64-windows/lib"
        "/vcpkg_installed/x64-windows/debug/lib"
        "/vcpkg_installed/x64-windows/lib"
)

if(FFMPEG_INCLUDE_DIR)
    target_include_directories(media_lib
        PUBLIC
            ""
    )
endif()

# ==================== 链接依赖 ====================
target_link_libraries(media_lib
    PUBLIC
        common_lib
)

if(AVCODEC_LIBRARY)
    target_link_libraries(media_lib PUBLIC "")
endif()

if(AVUTIL_LIBRARY)
    target_link_libraries(media_lib PUBLIC "")
endif()

# ==================== 平台配置 ====================
if(WIN32)
    target_compile_definitions(media_lib PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

if(MSVC)
    add_compile_options("/utf-8")
endif()

# ==================== 测试子目录 ====================
if(BUILD_MEDIA_TESTS AND EXISTS "/test")
    enable_testing()
    add_subdirectory(test)
endif()