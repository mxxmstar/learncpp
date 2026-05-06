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

# 测试4: Inference 模块单元测试
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_inference.cpp")
    add_executable(test_inference
        test_inference.cpp
    )

    target_link_libraries(test_inference
        PRIVATE
            alg_lib
            common_lib
            openvino::runtime
            ${CMAKE_THREAD_LIBS_INIT}
    )

    target_include_directories(test_inference
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_compile_features(test_inference PRIVATE cxx_std_20)
endif()

# 示例5: Inference 模块使用示例
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/inference_example.cpp")
    add_executable(inference_example
        inference_example.cpp
    )

    target_link_libraries(inference_example
        PRIVATE
            alg_lib
            common_lib
            openvino::runtime
            ${CMAKE_THREAD_LIBS_INIT}
    )

    target_include_directories(inference_example
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_compile_features(inference_example PRIVATE cxx_std_20)
endif()

# 测试6: OpenVINO Frontend 诊断工具
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_openvino_frontend.cpp")
    add_executable(test_openvino_frontend
        test_openvino_frontend.cpp
    )

    target_link_libraries(test_openvino_frontend
        PRIVATE
            openvino::runtime
    )

    target_compile_features(test_openvino_frontend PRIVATE cxx_std_20)
endif()

# 测试7: Decoder-Inference 流水线测试（零拷贝优化）
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_decoder_inference_pipeline.cpp")
    add_executable(test_decoder_inference_pipeline
        test_decoder_inference_pipeline.cpp
    )

    target_link_libraries(test_decoder_inference_pipeline
        PRIVATE
            alg_lib
            decoder_lib
            common_lib
            openvino::runtime
            ${CMAKE_THREAD_LIBS_INIT}
    )

    target_include_directories(test_decoder_inference_pipeline
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_compile_features(test_decoder_inference_pipeline PRIVATE cxx_std_20)
endif()

# ============================================================================
# OpenVINO DLL 自动复制配置
# ============================================================================
if(WIN32 AND TARGET openvino::runtime)
    # 根据构建类型确定 DLL 目录和文件名
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_CFG_INTDIR STREQUAL "Debug")
        set(OPENVINO_DLL_DIR "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/debug/bin")
        set(OPENVINO_DLL_SUFFIX "d")
        set(TBB_DLL_NAME "tbb12_debug.dll")
        set(PUGIXML_DLL_NAME "pugixml.dll")  # pugixml 没有 debug 后缀
    else()
        set(OPENVINO_DLL_DIR "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/bin")
        set(OPENVINO_DLL_SUFFIX "")
        set(TBB_DLL_NAME "tbb12.dll")
        set(PUGIXML_DLL_NAME "pugixml.dll")
    endif()
    
    # 定义需要复制的 OpenVINO DLL 列表
    set(OPENVINO_REQUIRED_DLLS
        "openvino${OPENVINO_DLL_SUFFIX}.dll"
        "openvino_intel_cpu_plugin${OPENVINO_DLL_SUFFIX}.dll"
        "openvino_ir_frontend${OPENVINO_DLL_SUFFIX}.dll"
    )
    
    # 可选的 DLL（如果存在则复制）
    set(OPENVINO_OPTIONAL_DLLS
        "openvino_intel_gpu_plugin${OPENVINO_DLL_SUFFIX}.dll"
        "openvino_onnx_frontend${OPENVINO_DLL_SUFFIX}.dll"
        "openvino_c${OPENVINO_DLL_SUFFIX}.dll"
        "${TBB_DLL_NAME}"
        "${PUGIXML_DLL_NAME}"
    )
    
    # 为所有使用 OpenVINO 的目标添加后构建命令
    foreach(target_name inference_example test_inference test_openvino_frontend test_decoder_inference_pipeline)
        if(TARGET ${target_name})
            # 复制必需的 DLL
            foreach(dll_name ${OPENVINO_REQUIRED_DLLS})
                add_custom_command(TARGET ${target_name} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${OPENVINO_DLL_DIR}/${dll_name}"
                    $<TARGET_FILE_DIR:${target_name}>
                    COMMENT "Copying ${dll_name} to output directory"
                    VERBATIM
                )
            endforeach()
            
            # 复制可选的 DLL
            foreach(dll_name ${OPENVINO_OPTIONAL_DLLS})
                add_custom_command(TARGET ${target_name} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${OPENVINO_DLL_DIR}/${dll_name}"
                    $<TARGET_FILE_DIR:${target_name}>
                    COMMENT "Copying optional ${dll_name} to output directory"
                    VERBATIM
                )
            endforeach()
        endif()
    endforeach()
    
    message(STATUS "OpenVINO DLL auto-copy configured for Debug/Release builds")
    message(STATUS "  DLL directory: ${OPENVINO_DLL_DIR}")
    message(STATUS "  DLL suffix: '${OPENVINO_DLL_SUFFIX}'")
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

if(TARGET test_inference)
    add_test(NAME test_inference
        COMMAND test_inference
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
if(TARGET test_inference)
    message(STATUS "  - test_inference (unit test)")
endif()
if(TARGET inference_example)
    message(STATUS "  - inference_example (example program)")
endif()
