# modules/media/puller/test/CMakeLists.txt
# puller 子模块测试构建。

if(NOT BUILD_PULLER_TESTS)
    return()
endif()

enable_testing()

file(GLOB PULLER_TEST_SOURCES "*.cpp")

foreach(test_file ${PULLER_TEST_SOURCES})
    get_filename_component(test_name ${test_file} NAME_WE)
    set(test_target "test_puller_${test_name}")

    add_executable(${test_target} ${test_file})

    target_include_directories(${test_target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../defines/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../stream/include
    )

    if(FFMPEG_INCLUDE_DIR)
        target_include_directories(${test_target} PRIVATE "${FFMPEG_INCLUDE_DIR}")
    endif()

    target_link_libraries(${test_target} PRIVATE
        media_puller_lib
        media_lib
        common_lib
        Boost::asio
    )

    if(AVCODEC_LIBRARY)
        target_link_libraries(${test_target} PRIVATE "${AVCODEC_LIBRARY}")
    endif()
    if(AVFORMAT_LIBRARY)
        target_link_libraries(${test_target} PRIVATE "${AVFORMAT_LIBRARY}")
    endif()
    if(AVUTIL_LIBRARY)
        target_link_libraries(${test_target} PRIVATE "${AVUTIL_LIBRARY}")
    endif()

    add_test(NAME ${test_target} COMMAND ${test_target})
    message(STATUS "Added puller test: ${test_target}")
endforeach()
