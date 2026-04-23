# HTTP Client 模块
project(http_client_lib)

# 收集源文件
file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE HEADERS "include/*.h")

# 创建库
add_library(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# 设置包含目录
target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# 查找依赖
find_package(Boost REQUIRED COMPONENTS json system)

# 链接依赖
target_link_libraries(${PROJECT_NAME} PUBLIC
    Boost::json
    Boost::system
    Boost::asio
    log_lib  # 日志模块
)

# 编译选项
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
endif()
