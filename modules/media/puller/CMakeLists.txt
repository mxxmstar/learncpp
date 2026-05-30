# modules/media/puller/CMakeLists.txt
# puller 子模块：基于 FFmpeg 的拉流器。

option(BUILD_PULLER_TESTS "Build media puller tests" ON)

file(GLOB PULLER_SOURCES "src/*.cpp")
add_library(media_puller_lib STATIC ${PULLER_SOURCES})

target_include_directories(media_puller_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_include_directories(media_puller_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../defines/include>
)

if(FFMPEG_INCLUDE_DIR)
    target_include_directories(media_puller_lib
        PUBLIC "${FFMPEG_INCLUDE_DIR}"
    )
endif()

target_link_libraries(media_puller_lib
    PUBLIC
        media_defines_lib
        common_lib
)

if(AVCODEC_LIBRARY)
    target_link_libraries(media_puller_lib PUBLIC "${AVCODEC_LIBRARY}")
endif()
if(AVFORMAT_LIBRARY)
    target_link_libraries(media_puller_lib PUBLIC "${AVFORMAT_LIBRARY}")
endif()
if(AVUTIL_LIBRARY)
    target_link_libraries(media_puller_lib PUBLIC "${AVUTIL_LIBRARY}")
endif()

if(WIN32)
    target_compile_definitions(media_puller_lib PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
endif()
if(MSVC)
    add_compile_options("/utf-8")
endif()

# ==================== 测试子目录 ====================
if(BUILD_PULLER_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test")
    add_subdirectory(test)
endif()
