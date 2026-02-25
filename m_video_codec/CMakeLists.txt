cmake_minimum_required(VERSION 3.18)
project(m_video_codec)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 使用 FindFFMPEG 模块（传统方式）
find_package(ffmpeg REQUIRED)

# 源文件
file(GLOB_RECURSE VIDEO_CODEC_SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_library(m_video_codec STATIC ${VIDEO_CODEC_SOURCES})

target_include_directories(m_video_codec PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include
    ${FFMPEG_INCLUDE_DIRS}
)

target_link_libraries(m_video_codec PUBLIC
    ${FFMPEG_LIBRARIES}
    spdlog::spdlog
)

if(WIN32)
    target_compile_definitions(m_video_codec PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
endif()
