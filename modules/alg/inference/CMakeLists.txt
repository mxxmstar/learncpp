# Inference 模块 CMakeLists.txt

cmake_minimum_required(VERSION 3.15)

# 查找 OpenVINO
find_package(OpenVINO REQUIRED)

# 收集源文件
file(GLOB INFERENCE_SOURCES
    "src/*.cpp"
)

# 收集头文件
file(GLOB INFERENCE_HEADERS
    "include/alg/inference/*.h"
)

# 创建库
add_library(alg_inference ${INFERENCE_SOURCES} ${INFERENCE_HEADERS})

# 设置包含目录
target_include_directories(alg_inference
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# 链接依赖
target_link_libraries(alg_inference
    PUBLIC
        openvino::runtime
        spdlog::spdlog
        common_lib  # 用于日志宏
    PRIVATE
        ${CMAKE_THREAD_LIBS_INIT}
)

# 设置 C++20 标准
target_compile_features(alg_inference PUBLIC cxx_std_20)

# 安装规则
install(TARGETS alg_inference
    EXPORT alg_inference-targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(DIRECTORY include/
    DESTINATION include
)

# 可选：编译示例程序
if(BUILD_ALG_TESTS)
    add_executable(inference_example examples/inference_example.cpp)
    target_link_libraries(inference_example
        PRIVATE
            alg_inference
            ${CMAKE_THREAD_LIBS_INIT}
    )
    
    # 设置 C++20 标准
    target_compile_features(inference_example PRIVATE cxx_std_20)
    
    # 复制示例到 bin 目录
    set_target_properties(inference_example PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
endif()
