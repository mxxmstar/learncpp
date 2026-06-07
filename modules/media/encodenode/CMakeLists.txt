# modules/media/encodenode/CMakeLists.txt
# encodenode submodule: asynchronous MediaFrame -> MediaPacket encoder node.

option(BUILD_ENCODENODE_TESTS "Build media encodenode tests" ON)

file(GLOB ENCODENODE_SOURCES "src/*.cpp")
add_library(media_encodenode_lib STATIC ${ENCODENODE_SOURCES})

target_include_directories(media_encodenode_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(media_encodenode_lib
    PUBLIC
        media_encoder_lib
        media_defines_lib
        common_lib
)

if(WIN32)
    target_compile_definitions(media_encodenode_lib PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
endif()
if(MSVC)
    add_compile_options("/utf-8")
endif()

if(BUILD_ENCODENODE_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test")
    add_subdirectory(test)
endif()
