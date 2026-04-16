# FFmpeg 推流工具模块
cmake_minimum_required(VERSION 3.15)

# 查找 FFmpeg 可执行文件
set(FFMPEG_PATH_NORMALIZED "")

if(WIN32)
    # Windows: 直接检查已知路径
    set(FFMPEG_CANDIDATE "${CMAKE_SOURCE_DIR}/tools/win32/ffmpeg-2025-05-01-git-707c04fe06-full_build/ffmpeg.exe")
    if(EXISTS "${FFMPEG_CANDIDATE}")
        set(FFMPEG_PATH_NORMALIZED "${FFMPEG_CANDIDATE}")
        message(STATUS "Found FFmpeg: ${FFMPEG_PATH_NORMALIZED}")
    else()
        message(WARNING "FFmpeg not found at: ${FFMPEG_CANDIDATE}")
    endif()
else()
    # Linux: 直接检查已知路径
    set(FFMPEG_CANDIDATE "${CMAKE_SOURCE_DIR}/tools/linux/ffmpeg_8_0/ffmpeg")
    if(EXISTS "${FFMPEG_CANDIDATE}")
        set(FFMPEG_PATH_NORMALIZED "${FFMPEG_CANDIDATE}")
        message(STATUS "Found FFmpeg: ${FFMPEG_PATH_NORMALIZED}")
    else()
        message(WARNING "FFmpeg not found at: ${FFMPEG_CANDIDATE}")
    endif()
endif()

# 创建静态库
add_library(ffmpeg_opt_lib STATIC
    src/ffmpeg_opt.cpp
)

# 设置包含目录
target_include_directories(ffmpeg_opt_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 链接依赖库
target_link_libraries(ffmpeg_opt_lib PUBLIC
    log_lib
)

# 定义 FFmpeg 路径宏
if(FFMPEG_PATH_NORMALIZED)
    target_compile_definitions(ffmpeg_opt_lib PRIVATE
        FFMPEG_PATH="${FFMPEG_PATH_NORMALIZED}"
    )
    message(STATUS "FFmpeg path macro defined: ${FFMPEG_PATH_NORMALIZED}")
else()
    message(WARNING "FFmpeg path macro not defined. Runtime detection will be used.")
endif()

# 设置 C++ 标准
set_target_properties(ffmpeg_opt_lib PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

# 如果启用测试，添加测试子目录
if(BUILD_TESTS)
    add_subdirectory(test)
endif()
