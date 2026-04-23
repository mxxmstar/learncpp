# WebSocket 模块
project(websocket_lib)

file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE HEADERS "include/*.h")

add_library(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

find_package(Boost REQUIRED COMPONENTS json system)

target_link_libraries(${PROJECT_NAME} PUBLIC
    Boost::json
    Boost::system
    Boost::asio
    log_lib
    http_server_lib  # WebSocket 依赖 HTTP Server（用于握手）
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
endif()
