# Service 模块 - 所有 Service 实现
project(service_module)

# 创建 service_lib（只包含 IService 接口头文件）
add_library(service_lib INTERFACE)

target_include_directories(service_lib
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# 添加子模块
add_subdirectory(http_server)
add_subdirectory(http_client)
add_subdirectory(zlm)
