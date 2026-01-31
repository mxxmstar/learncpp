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
    set(VCPKG_INSTALLED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed")
endif()

# 查找依赖
# find_package(fmt CONFIG REQUIRED)
# 1. 查找 spdlog（无额外组件）
find_package(spdlog CONFIG REQUIRED)
# 2. 查找 Boost，并指定需要的组件（asio 是 header-only，system 需要链接）
find_package(Boost REQUIRED COMPONENTS asio system uuid json process)
# 3. 查找 async_simple
find_package(async_simple CONFIG REQUIRED)

# 添加源文件
file(GLOB_RECURSE SOURCES "src/*.cpp")

# 定义预处理器宏来解决 WinSock 冲突
if(WIN32)
    add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

add_executable(${PROJECT_NAME} ${SOURCES} "src/zlmediakit/zlm_hookserver.cpp")

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
            async_simple::async_simple
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
            async_simple::async_simple
    )
endif()

# 创建一个库，以便测试可以链接到它
file(GLOB_RECURSE LIB_SOURCES "src/*.cpp")
add_library(myapp_lib ${LIB_SOURCES} "src/zlmediakit/zlm_httpclient.cpp" "src/zlmediakit/zlm_hookserver.cpp")
target_include_directories(myapp_lib PUBLIC include)

# 设置预处理器定义
if(WIN32)
    target_compile_definitions(myapp_lib PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
endif()
if(WIN32)
    target_link_libraries(myapp_lib
        PUBLIC
            spdlog::spdlog
            Boost::asio
            Boost::system
            Boost::uuid
            Boost::json
            Boost::process
            async_simple::async_simple
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
    )
endif()

# 如果启用测试构建，则添加测试
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()


