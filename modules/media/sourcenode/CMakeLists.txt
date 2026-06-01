# modules/media/sourcenode/CMakeLists.txt
# sourcenode 子模块：PullerSourceNode — 拉流 + 解码一体化源节点。

option(BUILD_SOURCENODE_TESTS "Build sourcenode tests" ON)

add_library(media_sourcenode_lib INTERFACE)

target_include_directories(media_sourcenode_lib
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(media_sourcenode_lib
    INTERFACE
        media_puller_lib
        media_decoder_lib
        media_defines_lib
        common_lib
        Boost::asio
)

# ==================== 测试子目录 ====================
if(BUILD_SOURCENODE_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test")
    add_subdirectory(test)
endif()
