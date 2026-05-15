# modules/osd/CMakeLists.txt
cmake_minimum_required(VERSION 3.18)
project(osd_module LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_OSD_TESTS "Build osd module tests" OFF)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)

file(GLOB OSD_SOURCES "src/*.cpp" "src/*/*.cpp")

add_library(osd_lib STATIC ${OSD_SOURCES})

target_include_directories(osd_lib 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(osd_lib
    PUBLIC
        common_lib
)

if(WIN32)
    target_compile_definitions(osd_lib PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

if(BUILD_OSD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test")
    enable_testing()
    add_subdirectory(test)
endif()