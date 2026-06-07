# modules/media/encoder/CMakeLists.txt
# encoder submodule: FFmpeg software encoder.

option(BUILD_ENCODER_TESTS "Build media encoder tests" ON)

file(GLOB ENCODER_SOURCES "src/*.cpp")
add_library(media_encoder_lib STATIC ${ENCODER_SOURCES})

target_include_directories(media_encoder_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_include_directories(media_encoder_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../defines/include>
)

if(FFMPEG_INCLUDE_DIR)
    target_include_directories(media_encoder_lib
        PUBLIC "${FFMPEG_INCLUDE_DIR}"
    )
endif()

target_link_libraries(media_encoder_lib
    PUBLIC
        media_defines_lib
        common_lib
)

if(AVCODEC_LIBRARY)
    target_link_libraries(media_encoder_lib PUBLIC "${AVCODEC_LIBRARY}")
endif()
if(AVUTIL_LIBRARY)
    target_link_libraries(media_encoder_lib PUBLIC "${AVUTIL_LIBRARY}")
endif()

if(WIN32)
    target_compile_definitions(media_encoder_lib PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
endif()
if(MSVC)
    add_compile_options("/utf-8")
endif()

if(BUILD_ENCODER_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test")
    add_subdirectory(test)
endif()
