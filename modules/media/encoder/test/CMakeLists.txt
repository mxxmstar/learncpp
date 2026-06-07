# modules/media/encoder/test/CMakeLists.txt

if(NOT BUILD_ENCODER_TESTS)
    return()
endif()

enable_testing()

file(GLOB ENCODER_TEST_SOURCES "*.cpp")

foreach(test_file ${ENCODER_TEST_SOURCES})
    get_filename_component(test_name ${test_file} NAME_WE)
    set(test_target "test_encoder_${test_name}")

    add_executable(${test_target} ${test_file})

    target_include_directories(${test_target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../defines/include
    )

    if(FFMPEG_INCLUDE_DIR)
        target_include_directories(${test_target} PRIVATE "${FFMPEG_INCLUDE_DIR}")
    endif()

    target_link_libraries(${test_target} PRIVATE
        media_encoder_lib
        media_defines_lib
        common_lib
    )

    if(AVCODEC_LIBRARY)
        target_link_libraries(${test_target} PRIVATE "${AVCODEC_LIBRARY}")
    endif()
    if(AVUTIL_LIBRARY)
        target_link_libraries(${test_target} PRIVATE "${AVUTIL_LIBRARY}")
    endif()

    add_test(NAME ${test_target} COMMAND ${test_target})
    message(STATUS "Added encoder test: ${test_target}")
endforeach()
