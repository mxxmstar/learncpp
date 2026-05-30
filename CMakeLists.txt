if(MSVC)
    add_compile_options(/utf-8)
endif()
cmake_minimum_required(VERSION 3.18)

# ⚠️ 重要：vcpkg toolchain 必须在 project() 之前设置
# 自动检测本地 vcpkg（如果存在）
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE FILEPATH "Vcpkg toolchain file")
    message(STATUS "Using vcpkg toolchain: ${CMAKE_TOOLCHAIN_FILE}")
endif()

project(MySelfContainedApp LANGUAGES CXX)
# 设置 CMake 策略以消除 Boost 警告
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows MSVC: 添加 /FS 标志以支持并行编译时共享 PDB 文件
if(MSVC)
    add_compile_options(/FS)
    message(STATUS "Added /FS flag for parallel compilation with shared PDB")
endif()

# 添加编译开关来控制是否构建测试
option(BUILD_TESTS "Build tests" ON)

# 编译开关：完整视频管线（含解码/推流/AI），关闭则仅编译核心模块
option(BUILD_FULL_PIPELINE "Build full video pipeline (decoder, AI, pusher, etc.)" OFF)

# 设置可执行文件输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# gRPC 模块已迁移到 modules/alg/grpc/
# add_subdirectory(grpc)

# ==================== 查找依赖（所有模块共享） ====================
# 0. 查找 fmt（log 模块需要）
find_package(fmt CONFIG REQUIRED)
# 1. 查找 spdlog（无额外组件）
find_package(spdlog CONFIG REQUIRED)
# 2. 查找 Boost，并指定需要的组件（lockfree/beast/asio 为 header-only）
find_package(Boost REQUIRED COMPONENTS asio system uuid json process pool filesystem lockfree beast)
# 3. 查找 yaml-cpp（common 模块需要）
find_package(yaml-cpp CONFIG REQUIRED)

# ==================== 仅全管线模式需要的依赖 ====================
if(BUILD_FULL_PIPELINE)
    if(WIN32)
        find_package(unofficial-sqlite3 CONFIG REQUIRED)
    else()
        find_package(SQLite3 REQUIRED)
    endif()
    find_package(ffmpeg REQUIRED)
    # 设置 FFmpeg 库变量（供测试使用）
    if(WIN32)
        set(FFMPEG_LIBRARIES 
            "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/debug/lib/avcodec.lib"
            "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/debug/lib/avformat.lib"
            "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/debug/lib/avutil.lib"
            "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/debug/lib/swresample.lib"
            "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/debug/lib/swscale.lib"
        )
    else()
        set(FFMPEG_LIBRARIES avcodec avformat avutil swresample swscale)
    endif()
    find_package(OpenCV REQUIRED)
    if(NOT DEFINED OpenCV_LIBS)
        set(OpenCV_LIBS opencv_core opencv_imgproc opencv_imgcodecs opencv_videoio opencv_highgui)
    endif()
    find_package(gRPC CONFIG REQUIRED)
    find_package(Protobuf CONFIG REQUIRED)
    find_package(OpenVINO REQUIRED)
    find_package(nlohmann_json CONFIG REQUIRED)
    find_package(async_simple CONFIG REQUIRED)
endif()

# 定义预处理器宏来解决 WinSock 冲突
if(WIN32)
    add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

# 添加 FFmpeg 所需的宏定义
add_compile_definitions(__STDC_CONSTANT_MACROS)
add_compile_definitions(__STDC_LIMIT_MACROS)

if(MSVC)
    add_compile_options(/Zm500) # 将内存上限提高到默认值的 200%
    #add_compile_options(/ZI)
endif()

# 添加模块化库（按依赖顺序）
# ==================== 核心模块（始终编译） ====================
add_subdirectory(modules/common)  # common 模块（IService 接口，log, config）
add_subdirectory(modules/puller)  # puller 模块依赖 common
add_subdirectory(modules/media)   # media 模块（纯C媒体包定义 + 队列/池/分配器）

# ==================== 全管线模块（仅 BUILD_FULL_PIPELINE=ON） ====================
if(BUILD_FULL_PIPELINE)
    add_subdirectory(modules/net)
    add_subdirectory(modules/camera)
    add_subdirectory(modules/decoder)
    add_subdirectory(modules/preprocess)
    add_subdirectory(modules/osd)
    add_subdirectory(modules/postprocess)
    add_subdirectory(modules/grpc)
    add_subdirectory(modules/alg)
    add_subdirectory(modules/videopipeline)
    add_subdirectory(modules/sqlite)
    add_subdirectory(modules/zlmediakit)
    add_subdirectory(modules/service)
    add_subdirectory(modules/application)
    add_subdirectory(modules/api)
    add_subdirectory(modules/ffmpeg_opt)
    add_subdirectory(modules/pusher)
endif()

# 主程序只包含 main.cpp，其他功能通过模块链接
add_executable(${PROJECT_NAME} apps/main.cpp)

target_include_directories(${PROJECT_NAME} PRIVATE include)

# 添加 gRPC 头文件路径
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/grpc/include)

# 针对 Windows 平台链接特定库
if(WIN32)
    # ===== 始终链接的核心模块 =====
    target_link_libraries(${PROJECT_NAME}
        PRIVATE
            common_lib        # 通用service接口 + log
            common_lib
            puller_lib        # 拉流器模块
            #stream_lib        # 流媒体输入源模块
            media_lib         # 媒体包模块
            spdlog::spdlog
            Boost::asio
            Boost::system
            Boost::uuid
            Boost::json
            Boost::process
            Boost::pool
            yaml-cpp::yaml-cpp
    )

    # ===== 全管线模式额外链接 =====
    if(BUILD_FULL_PIPELINE)
        target_link_libraries(${PROJECT_NAME}
            PRIVATE
                net_lib
                camera_lib
                decoder_lib
                preprocess_lib
                postprocess_lib
                grpc_lib
                alg_lib
                videopipeline_lib
                sqlite_lib
                zlmediakit_lib
                api_lib
                application_lib
                pusher_lib
                async_simple::async_simple
                ${OpenCV_LIBS}
                ${FFMPEG_LIBRARIES}
                ntdll
                user32
                shell32
        )
    endif()
else()
    target_link_libraries(${PROJECT_NAME}
        PRIVATE
            common_lib
            puller_lib
            #stream_lib
            media_lib
            spdlog::spdlog
            Boost::asio
            Boost::system
            Boost::uuid
            Boost::json
            Boost::process
            Boost::pool
            yaml-cpp::yaml-cpp
    )
    if(BUILD_FULL_PIPELINE)
        target_link_libraries(${PROJECT_NAME}
            PRIVATE
                async_simple::async_simple
                sqlite3
        )
    endif()
endif()

# myapp_lib 已废弃，功能由各模块提供
# 保留仅为了向后兼容，实际应该直接使用各模块的库
# file(GLOB_RECURSE LIB_SOURCES "src/*.cpp")
# add_library(myapp_lib ${LIB_SOURCES})
# target_include_directories(myapp_lib PUBLIC include)
# target_include_directories(myapp_lib PUBLIC ${CMAKE_SOURCE_DIR}/grpc/include)

# myapp_lib 已废弃，以下配置注释掉
# # 设置预处理器定义
# if(WIN32)
#     target_compile_definitions(myapp_lib PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
# endif()

# Windows 特定配置
# if(WIN32)
#     set(SQLITE3_LIB_PATH "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/debug/lib/sqlite3.lib")
#     if(NOT EXISTS "${SQLITE3_LIB_PATH}")
#         set(SQLITE3_LIB_PATH "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/lib/sqlite3.lib")
#     endif()
# endif()

# # 链接依赖库
# target_link_libraries(myapp_lib
#     PUBLIC
#         common_lib
#         spdlog::spdlog
#         Boost::asio
#         Boost::system
#         Boost::uuid
#         Boost::json
#         Boost::process
#         async_simple::async_simple
#         yaml-cpp::yaml-cpp
#         ${OpenCV_LIBS}
#         grpc_lib
# )

# # Windows 特定链接
# if(WIN32)
#     target_link_libraries(myapp_lib
#         PUBLIC
#             ${FFMPEG_LIBRARIES}
#             "${SQLITE3_LIB_PATH}"
#             ntdll
#             user32
#             shell32
#     )
# else()
#     target_link_libraries(myapp_lib
#         PUBLIC
#             spdlog::spdlog
#             Boost::asio
#             Boost::system
#             Boost::uuid
#             Boost::json
#             Boost::process
#             async_simple::async_simple
#             yaml-cpp::yaml-cpp
#             sqlite3
#     )
# endif()

# 测试由各模块自己的 CMakeLists.txt 管理
# 不再自动编译 test 目录，方便模块化开发和测试
# if(BUILD_TESTS)
#     enable_testing()
#     set(SQLITE3_LIB_PATH_TEST "${SQLITE3_LIB_PATH}" CACHE PATH "SQLite3 library path for tests")
#     add_subdirectory(test)
# endif()

# ==================== Application Framework 示例应用 ====================
if(BUILD_TESTS)
    # 创建使用 Application 框架的示例应用
    add_executable(app_with_framework 
        apps/app_with_framework.cpp
    )
    
    target_include_directories(app_with_framework 
        PRIVATE 
            include
            modules/application/include
            modules/service/include
    )
    
    if(WIN32)
        target_link_libraries(app_with_framework
            PRIVATE
                common_lib
                puller_lib
                #stream_lib
                media_lib
                spdlog::spdlog
                Boost::asio
                Boost::system
                Boost::uuid
                Boost::json
                Boost::process
                Boost::pool
                yaml-cpp::yaml-cpp
        )
        if(BUILD_FULL_PIPELINE)
            target_link_libraries(app_with_framework
                PRIVATE
                    net_lib
                    camera_lib
                    decoder_lib
                    preprocess_lib
                    postprocess_lib
                    grpc_lib
                    alg_lib
                    videopipeline_lib
                    sqlite_lib
                    zlmediakit_lib
                    api_lib
                    service_lib
                    application_lib
                    http_server_service_lib
                    http_client_pool_service_lib
                    zlm_service_lib
                    async_simple::async_simple
                    ${OpenCV_LIBS}
                    ${FFMPEG_LIBRARIES}
                    ntdll
                    user32
                    shell32
            )
        endif()
    else()
        target_link_libraries(app_with_framework
            PRIVATE
                common_lib
                puller_lib
                #stream_lib
                media_lib
                spdlog::spdlog
                Boost::asio
                Boost::system
                yaml-cpp::yaml-cpp
        )
        if(BUILD_FULL_PIPELINE)
            target_link_libraries(app_with_framework
                PRIVATE
                    application_lib
                    async_simple::async_simple
            )
        endif()
    endif()
endif()




