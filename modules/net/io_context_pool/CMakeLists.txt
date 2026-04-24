# IO Context Pool 模块
project(io_context_pool_lib)

file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE HEADERS "include/*.h")

add_library(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

find_package(Boost REQUIRED COMPONENTS system)

target_link_libraries(${PROJECT_NAME} PUBLIC
    Boost::system
    Boost::asio
    common_lib
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _WIN32_WINNT=0x0601 WIN32_LEAN_AND_MEAN NOMINMAX)
endif()
