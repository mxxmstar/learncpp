# modules/media/decoder/CMakeLists.txt
# decoder 子模块：基于 FFmpeg 的软件解码器。

option(BUILD_DECODER_TESTS "Build media decoder tests" ON)

file(GLOB DECODER_SOURCES "src/*.cpp")
add_library(media_decoder_lib STATIC ${DECODER_SOURCES})

target_include_directories(media_decoder_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_include_directories(media_decoder_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../defines/include>
)

target_link_libraries(media_decoder_lib
    PUBLIC
        media_defines_lib
        common_lib
)

if(WIN32)
    target_compile_definitions(media_decoder_lib PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
endif()
if(MSVC)
    add_compile_options("/utf-8")
endif()

# ==================== 测试子目录 ====================
if(BUILD_DECODER_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test")
    add_subdirectory(test)
endif()
