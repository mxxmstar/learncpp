cmake_minimum_required(VERSION 3.18)
project(common_lib LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 收集源文件（包括 log 和 config）
file(GLOB_RECURSE LIB_SOURCES 
    "src/*.cpp"
    "include/common/log/*.h"
    "include/common/config/*.h"
    "include/common/pool/*.hpp"
    "include/common/process/*.h"
    "include/common/runtime/*.h"
    "include/common/queue/*.h"
    "include/common/queue/third_part/*.h"
)

# 创建库
add_library(${PROJECT_NAME} ${LIB_SOURCES})

# 设置包含目录
target_include_directories(${PROJECT_NAME} 
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 链接依赖
find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS json system filesystem process lockfree asio)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        spdlog::spdlog
        fmt::fmt
        yaml-cpp::yaml-cpp
        Boost::json
        Boost::system
        Boost::filesystem
        Boost::lockfree
        Boost::asio
    PRIVATE
        Boost::process
)

# Windows 特定配置
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PUBLIC _WIN32_WINNT=0x0601 WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

# 测试（可选）
if(BUILD_TESTS)
    file(GLOB TEST_SOURCES "test/*.cpp")
    if(TEST_SOURCES)
        foreach(TEST_FILE ${TEST_SOURCES})
            get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
            add_executable(test_${TEST_NAME} ${TEST_FILE})
            target_link_libraries(test_${TEST_NAME} PRIVATE ${PROJECT_NAME})
        endforeach()
    endif()
endif()
