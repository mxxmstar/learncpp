cmake_minimum_required(VERSION 3.18)
project(m_rtsp_server)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 源文件
file(GLOB_RECURSE RTSP_SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_executable(m_rtsp_server ${RTSP_SOURCES})

# 1. 依赖 m_video_codec（刚添加的静态库）
target_link_libraries(m_rtsp_server PRIVATE
    m_video_codec
)

# 2. 复用主工程的 net 模块（通过 myapp_lib）
target_link_libraries(m_rtsp_server PRIVATE
    myapp_lib
)

# 头文件目录
target_include_directories(m_rtsp_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/m_video_codec/include
)

# Windows 特定配置
if(WIN32)
    target_compile_definitions(m_rtsp_server PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(m_rtsp_server PRIVATE
        ntdll
        user32
        shell32
    )
endif()
