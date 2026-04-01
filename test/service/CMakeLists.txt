# test/service/CMakeLists.txt
# Service 架构测试

cmake_minimum_required(VERSION 3.18)

# 创建测试可执行文件
add_executable(test_service_arch
    test_service_arch.cpp
    ${CMAKE_SOURCE_DIR}/src/service/http_server_service.cpp
    ${CMAKE_SOURCE_DIR}/src/service/zlm_service.cpp
)

target_include_directories(test_service_arch PRIVATE 
    ${CMAKE_SOURCE_DIR}/include
)

# 添加 Windows 版本定义，避免编译警告
if(WIN32)
    target_compile_definitions(test_service_arch PRIVATE 
        _WIN32_WINNT=0x0A00  # Windows 10
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
endif()

target_link_libraries(test_service_arch PRIVATE 
    myapp_lib
    spdlog::spdlog
    yaml-cpp::yaml-cpp
    Boost::asio
    Boost::system
    Boost::json
    Boost::process
)

# 添加测试
add_test(NAME test_service_arch COMMAND test_service_arch)

message(STATUS "Added test: test_service_arch")
