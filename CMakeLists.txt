cmake_minimum_required(VERSION 3.18)
project(MySelfContainedApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置可执行文件输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# 自动检测本地 vcpkg（如果存在）
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
    # 默认安装到项目内的 vcpkg_installed/
    set(VCPKG_INSTALLED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed")
endif()

# 查找依赖
# find_package(fmt CONFIG REQUIRED)
# 1. 查找 spdlog（无额外组件）
find_package(spdlog CONFIG REQUIRED)
# 2. 查找 Boost，并指定需要的组件（asio 是 header-only，system 需要链接）
find_package(Boost REQUIRED COMPONENTS asio system)

# 添加源文件
set (SOURCES
    src/main.cpp
    src/log/logger.cpp
    src/net/asio_io_context_pool.cpp
)


add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE include)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        spdlog::spdlog
        Boost::asio
        Boost::system

)