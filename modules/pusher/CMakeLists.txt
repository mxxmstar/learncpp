cmake_minimum_required(VERSION 3.15)
project(pusher_lib LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_PUSHER_TESTS "Build pusher tests" ON)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)

file(GLOB PUSHER_SOURCES "src/*.cpp")

add_library(pusher_lib STATIC ${PUSHER_SOURCES})

target_include_directories(pusher_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(pusher_lib PUBLIC
    common_lib
    ffmpeg_opt_lib
)

if(WIN32)
    target_compile_definitions(pusher_lib PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
    target_compile_options(pusher_lib PRIVATE /utf-8)
endif()

if(BUILD_PUSHER_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test")
    enable_testing()
    add_subdirectory(test)
endif()