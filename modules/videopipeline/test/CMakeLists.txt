# modules/videopipeline/test/CMakeLists.txt
# VideoPipeline 模块测试配置

# 设置测试可执行文件输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

# 测试1: 单通道处理测试
add_executable(test_single_channel_processing
    test_single_channel_processing.cpp
)

target_link_libraries(test_single_channel_processing
    PRIVATE
        videopipeline_lib
        alg_lib        # base_algorithm 在 alg 模块
        postprocess_lib  # result_output 和 osd_renderer 在 postprocess 模块
        log_lib
)

target_include_directories(test_single_channel_processing
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# 测试2: 视频流水线基本测试
add_executable(test_video_pipeline
    test_video_pipeline.cpp
)

target_link_libraries(test_video_pipeline
    PRIVATE
        videopipeline_lib
        log_lib
)

target_include_directories(test_video_pipeline
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# 测试3: 视频流水线 gRPC 集成测试
add_executable(test_video_pipeline_grpc
    test_video_pipeline_grpc.cpp
)

target_link_libraries(test_video_pipeline_grpc
    PRIVATE
        videopipeline_lib
        alg_lib      # gRPC 算法处理器在 alg 模块
        grpc_lib     # gRPC 基础设施
        log_lib
)

target_include_directories(test_video_pipeline_grpc
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# 注册测试
enable_testing()

add_test(NAME test_single_channel_processing
    COMMAND test_single_channel_processing
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(NAME test_video_pipeline
    COMMAND test_video_pipeline
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(NAME test_video_pipeline_grpc
    COMMAND test_video_pipeline_grpc
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# 打印测试信息
message(STATUS "VideoPipeline module tests configured:")
message(STATUS "  - test_single_channel_processing")
message(STATUS "  - test_video_pipeline")
message(STATUS "  - test_video_pipeline_grpc")
