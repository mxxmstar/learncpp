# modules/videopipeline/CMakeLists.txt
cmake_minimum_required(VERSION 3.18)
project(videopipeline_module LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 测试开关
option(BUILD_VIDEOPIPELINE_TESTS "Build videopipeline module tests" ON)

# 设置库文件输出目录
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)

# 收集源文件
file(GLOB VIDEOPIPELINE_SOURCES "src/*.cpp")

# 创建静态库
add_library(videopipeline_lib STATIC ${VIDEOPIPELINE_SOURCES})

# 设置包含目录
target_include_directories(videopipeline_lib 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# 链接依赖
target_link_libraries(videopipeline_lib
    PUBLIC
        ${OpenCV_LIBS}
        decoder_lib
        preprocess_lib
        osd_lib       # OSD 渲染器
        postprocess_lib
        grpc_lib  # videopipeline 使用 gRPC 基础设施
        alg_lib  # alg_lib 包含 gRPC 算法处理器
        puller_lib  # videopipeline 依赖 puller
        pusher_lib  # 推流模块
        common_lib
        net_lib
        Boost::asio
        Boost::lockfree
        common_lib        # 通用service接口
)

# Windows 特定配置
if(WIN32)
    target_compile_definitions(videopipeline_lib PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

# 如果启用测试，添加测试子目录
if(BUILD_VIDEOPIPELINE_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test")
    enable_testing()
    add_subdirectory(test)
endif()
