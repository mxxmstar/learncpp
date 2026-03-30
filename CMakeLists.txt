cmake_minimum_required(VERSION 3.18)
project(MySelfContainedApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加编译开关来控制是否构建测试
option(BUILD_TESTS "Build tests" ON)

# 设置可执行文件输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# 自动检测本地 vcpkg（如果存在）
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
    # 默认安装到项目内的 vcpkg_installed/
    # set(VCPKG_INSTALLED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed")
endif()

# 查找依赖
# find_package(fmt CONFIG REQUIRED)
# 1. 查找 spdlog（无额外组件）
find_package(spdlog CONFIG REQUIRED)
# 2. 查找 Boost，并指定需要的组件（asio 是 header-only，system 需要链接）
find_package(Boost REQUIRED COMPONENTS asio system uuid json process pool filesystem)
# 3. 查找 async_simple
find_package(async_simple CONFIG REQUIRED)
# 4. 查找 yaml-cpp
find_package(yaml-cpp CONFIG REQUIRED)
# 5. 查找 FFmpeg（用于视频编解码模块） fmpeg 包使用的是传统的 FindFFMPEG 模块，而不是现代的 Config 模式。
find_package(ffmpeg REQUIRED)
# 6. 尝试查找 SQLite
find_package(sqlite3 CONFIG)
# 设置 SQLite3 库路径（供测试使用）
if(WIN32)
    set(SQLITE3_LIB_PATH "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/debug/lib/sqlite3.lib")
    if(NOT EXISTS "${SQLITE3_LIB_PATH}")
        set(SQLITE3_LIB_PATH "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/lib/sqlite3.lib")
    endif()
endif()

# 添加源文件
file(GLOB_RECURSE SOURCES "src/*.cpp")

# 定义预处理器宏来解决 WinSock 冲突
if(WIN32)
    add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

# 添加 FFmpeg 所需的宏定义
add_compile_definitions(__STDC_CONSTANT_MACROS)
add_compile_definitions(__STDC_LIMIT_MACROS)

if(MSVC)
    add_compile_options(/Zm500) # 将内存上限提高到默认值的 200%
    add_compile_options(/ZI)
endif()

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE include)

# 针对 Windows 平台链接特定库
if(WIN32)
    target_link_libraries(${PROJECT_NAME}
        PRIVATE
            spdlog::spdlog
            Boost::asio
            Boost::system
            Boost::uuid
            Boost::json
            Boost::process
            Boost::pool
            async_simple::async_simple
            yaml-cpp::yaml-cpp
            # sqlite3
            "${SQLITE3_LIB_PATH}"
            ntdll
            user32
            shell32
    )
else()
    target_link_libraries(${PROJECT_NAME}
        PRIVATE
            spdlog::spdlog
            Boost::asio
            Boost::system
            Boost::uuid
            Boost::json
            Boost::process
            Boost::pool
            async_simple::async_simple
            yaml-cpp::yaml-cpp
            sqlite3
    )
endif()

# 创建一个库，以便测试可以链接到它
file(GLOB_RECURSE LIB_SOURCES "src/*.cpp")
add_library(myapp_lib ${LIB_SOURCES})
target_include_directories(myapp_lib PUBLIC include)

# 设置预处理器定义
if(WIN32)
    target_compile_definitions(myapp_lib PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
endif()
if(WIN32)
    set(SQLITE3_LIB_PATH "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/debug/lib/sqlite3.lib")
    if(NOT EXISTS "${SQLITE3_LIB_PATH}")
        set(SQLITE3_LIB_PATH "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/lib/sqlite3.lib")
    endif()
    target_link_libraries(myapp_lib
        PUBLIC
            spdlog::spdlog
            Boost::asio
            Boost::system
            Boost::uuid
            Boost::json
            Boost::process
            async_simple::async_simple
            yaml-cpp::yaml-cpp
            # sqlite3
            "${SQLITE3_LIB_PATH}"
            ntdll
            user32
            shell32
    )
else()
    target_link_libraries(myapp_lib
        PUBLIC
            spdlog::spdlog
            Boost::asio
            Boost::system
            Boost::uuid
            Boost::json
            Boost::process
            async_simple::async_simple
            yaml-cpp::yaml-cpp
            sqlite3
    )
endif()

# 如果启用测试构建，则添加测试
if(BUILD_TESTS)
    enable_testing()
    # 传递 SQLite3 库路径给测试
    set(SQLITE3_LIB_PATH_TEST "${SQLITE3_LIB_PATH}" CACHE PATH "SQLite3 library path for tests")
    add_subdirectory(test)
endif()


