# grpc/CMakeLists.txt
# 独立的 gRPC 库

cmake_minimum_required(VERSION 3.18)
project(grpc_lib LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找 gRPC 和 Protobuf
find_package(gRPC CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS asio system)
find_package(OpenCV REQUIRED)

# Proto 文件路径
set(PROTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/proto")
set(GENERATED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/generated")

# 确保生成目录存在
file(MAKE_DIRECTORY ${GENERATED_DIR})

# 查找 protoc 和 grpc_cpp_plugin
find_program(PROTOC protoc PATHS ${CMAKE_PREFIX_PATH} NO_DEFAULT_PATH)
if(NOT PROTOC)
    find_program(PROTOC protoc)
endif()

find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin PATHS ${CMAKE_PREFIX_PATH} NO_DEFAULT_PATH)
if(NOT GRPC_CPP_PLUGIN)
    find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin)
endif()

if(NOT PROTOC)
    message(FATAL_ERROR "protoc not found")
endif()

if(NOT GRPC_CPP_PLUGIN)
    message(FATAL_ERROR "grpc_cpp_plugin not found")
endif()

message(STATUS "gRPC lib - Found protoc: ${PROTOC}")
message(STATUS "gRPC lib - Found grpc_cpp_plugin: ${GRPC_CPP_PLUGIN}")

# 生成 gRPC 代码（hello.proto）
set(GRPC_GENERATED_HEADERS
    "${GENERATED_DIR}/hello.pb.h"
    "${GENERATED_DIR}/hello.grpc.pb.h"
)

set(GRPC_GENERATED_SOURCES
    "${GENERATED_DIR}/hello.pb.cc"
    "${GENERATED_DIR}/hello.grpc.pb.cc"
)

# 生成视频处理 gRPC 代码（video_processing.proto）
set(VIDEO_GRPC_GENERATED_HEADERS
    "${GENERATED_DIR}/video_processing.pb.h"
    "${GENERATED_DIR}/video_processing.grpc.pb.h"
)

set(VIDEO_GRPC_GENERATED_SOURCES
    "${GENERATED_DIR}/video_processing.pb.cc"
    "${GENERATED_DIR}/video_processing.grpc.pb.cc"
)

# 添加自定义命令生成 hello 代码
add_custom_command(
    OUTPUT ${GRPC_GENERATED_HEADERS} ${GRPC_GENERATED_SOURCES}
    COMMAND ${PROTOC}
    ARGS --proto_path=${PROTO_DIR}
         --cpp_out=${GENERATED_DIR}
         --grpc_out=${GENERATED_DIR}
         --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
         ${PROTO_DIR}/hello.proto
    DEPENDS ${PROTO_DIR}/hello.proto
    COMMENT "Generating gRPC code from hello.proto"
    VERBATIM
)

# 添加自定义命令生成视频处理代码
add_custom_command(
    OUTPUT ${VIDEO_GRPC_GENERATED_HEADERS} ${VIDEO_GRPC_GENERATED_SOURCES}
    COMMAND ${PROTOC}
    ARGS --proto_path=${PROTO_DIR}
         --cpp_out=${GENERATED_DIR}
         --grpc_out=${GENERATED_DIR}
         --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
         ${PROTO_DIR}/video_processing.proto
    DEPENDS ${PROTO_DIR}/video_processing.proto
    COMMENT "Generating gRPC code from video_processing.proto"
    VERBATIM
)

# 收集所有源文件
set(GRPC_LIB_SOURCES
    src/grpc_server.cpp
    src/grpc_client.cpp
    src/hello_grpc_service.cpp
    src/video_grpc_client.cpp
    ${GRPC_GENERATED_SOURCES}
    ${VIDEO_GRPC_GENERATED_SOURCES}
)

# 创建静态库
add_library(grpc_lib STATIC ${GRPC_LIB_SOURCES})

# 设置 include 目录
target_include_directories(grpc_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${GENERATED_DIR}>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# 链接依赖库
target_link_libraries(grpc_lib
    PUBLIC
        gRPC::grpc++
        gRPC::grpc++_reflection
        protobuf::libprotobuf
    PRIVATE
        Boost::asio
        Boost::system
        ${OpenCV_LIBS}
)

# Windows 定义
if(WIN32)
    target_compile_definitions(grpc_lib PRIVATE
        _WIN32_WINNT=0x0A00
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        PROTOBUF_USE_DLLS
    )
    
    # 使用 UTF-8 编码
    target_compile_options(grpc_lib PRIVATE /utf-8)
endif()

message(STATUS "gRPC library configured successfully")
message(STATUS "Generated headers: ${GRPC_GENERATED_HEADERS}")
message(STATUS "Generated sources: ${GRPC_GENERATED_SOURCES}")
