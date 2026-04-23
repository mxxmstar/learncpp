# HTTP Client Pool Service 模块
project(http_client_pool_service_lib)

file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE HEADERS "include/*.h")

add_library(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

find_package(Boost REQUIRED COMPONENTS json system)
find_package(yaml-cpp CONFIG REQUIRED)

target_link_libraries(${PROJECT_NAME} PUBLIC
    Boost::json
    Boost::system
    Boost::asio
    yaml-cpp::yaml-cpp
    common_lib        # IService, log, config
    net_lib           # http_client_lib
    application_lib   # Application
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
endif()
