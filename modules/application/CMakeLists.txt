cmake_minimum_required(VERSION 3.18)
project(application_lib LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 收集源文件
file(GLOB_RECURSE APP_SOURCES "src/*.cpp")

# 创建库
add_library(${PROJECT_NAME} ${APP_SOURCES})

# 设置包含目录
target_include_directories(${PROJECT_NAME} 
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 链接依赖
# SignalHandler 使用日志宏输出信号处理信息
target_link_libraries(${PROJECT_NAME}
    PUBLIC
        common_lib     # SignalHandler 需要日志
        service_lib # IService 接口（已从 common 迁移到 service）
)

# Windows 特定配置
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()
