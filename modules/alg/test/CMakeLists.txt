# modules/alg/test/CMakeLists.txt
# ALG 模块测试配置（包含 gRPC 算法处理器测试）

# 设置测试可执行文件输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

# 测试1: video_grpc_client 测试
add_executable(test_grpc_video_client
    test_video_grpc_client.cpp
)

target_link_libraries(test_grpc_video_client
    PRIVATE
        grpc_lib
        alg_lib  # video_grpc_client 已移到 alg 模块
        common_lib
)

target_include_directories(test_grpc_video_client
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# 测试2: grpc algorithm 集成测试
add_executable(test_grpc_algorithm_integration
    test_grpc_algorithm_integration.cpp
)

target_link_libraries(test_grpc_algorithm_integration
    PRIVATE
        grpc_lib
        alg_lib
        common_lib
)

target_include_directories(test_grpc_algorithm_integration
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# 测试3: video_processor 测试（如果有对应的实现）
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_video_processor.cpp")
    add_executable(test_grpc_video_processor
        test_video_processor.cpp
    )

    target_link_libraries(test_grpc_video_processor
        PRIVATE
            grpc_lib
            alg_lib  # 需要 alg_lib 的 IAlgorithmProcessor
            common_lib
    )

    target_include_directories(test_grpc_video_processor
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()

# 注册测试
enable_testing()

add_test(NAME test_grpc_video_client
    COMMAND test_grpc_video_client
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(NAME test_grpc_algorithm_integration
    COMMAND test_grpc_algorithm_integration
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

if(TARGET test_grpc_video_processor)
    add_test(NAME test_grpc_video_processor
        COMMAND test_grpc_video_processor
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
endif()

# 打印测试信息
message(STATUS "ALG module tests configured:")
message(STATUS "  - test_grpc_video_client")
message(STATUS "  - test_grpc_algorithm_integration")
if(TARGET test_grpc_video_processor)
    message(STATUS "  - test_grpc_video_processor")
endif()
