# modules/media/pusher/CMakeLists.txt

option(BUILD_MEDIA_PUSHER_TESTS "Build media pusher tests" ON)

file(GLOB PUSHER_SOURCES "src/*.cpp")
add_library(media_pusher_lib STATIC ${PUSHER_SOURCES})

target_include_directories(media_pusher_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_include_directories(media_pusher_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../defines/include>
)

if(NOT FFMPEG_INCLUDE_DIR)
    find_path(FFMPEG_INCLUDE_DIR
        NAMES libavcodec/avcodec.h
        PATHS
            "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/include"
            /usr/include
            /usr/local/include
    )
endif()

if(NOT AVCODEC_LIBRARY)
    find_library(AVCODEC_LIBRARY
        NAMES avcodec
        PATHS
            "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/debug/lib"
            "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/lib"
    )
endif()

if(NOT AVFORMAT_LIBRARY)
    find_library(AVFORMAT_LIBRARY
        NAMES avformat
        PATHS
            "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/debug/lib"
            "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/lib"
    )
endif()

if(NOT AVUTIL_LIBRARY)
    find_library(AVUTIL_LIBRARY
        NAMES avutil
        PATHS
            "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/debug/lib"
            "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/lib"
    )
endif()

if(FFMPEG_INCLUDE_DIR)
    target_include_directories(media_pusher_lib PUBLIC "${FFMPEG_INCLUDE_DIR}")
endif()

target_link_libraries(media_pusher_lib
    PUBLIC
        media_defines_lib
        common_lib
)

if(AVCODEC_LIBRARY)
    target_link_libraries(media_pusher_lib PUBLIC "${AVCODEC_LIBRARY}")
endif()
if(AVFORMAT_LIBRARY)
    target_link_libraries(media_pusher_lib PUBLIC "${AVFORMAT_LIBRARY}")
endif()
if(AVUTIL_LIBRARY)
    target_link_libraries(media_pusher_lib PUBLIC "${AVUTIL_LIBRARY}")
endif()

if(WIN32)
    target_compile_definitions(media_pusher_lib PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
endif()
if(MSVC)
    target_compile_options(media_pusher_lib PRIVATE /utf-8)
endif()

if(BUILD_MEDIA_PUSHER_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test")
    add_subdirectory(test)
endif()
