# modules/media/encodenode/test/CMakeLists.txt

if(NOT BUILD_ENCODENODE_TESTS)
    return()
endif()

enable_testing()

file(GLOB ENCODENODE_TEST_SOURCES "*.cpp")

foreach(test_file ${ENCODENODE_TEST_SOURCES})
    get_filename_component(test_name ${test_file} NAME_WE)
    set(test_target "test_encodenode_${test_name}")

    add_executable(${test_target} ${test_file})

    target_include_directories(${test_target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../encoder/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../defines/include
    )

    target_link_libraries(${test_target} PRIVATE
        media_encodenode_lib
        media_encoder_lib
        media_defines_lib
        common_lib
    )

    add_test(NAME ${test_target} COMMAND ${test_target})
    message(STATUS "Added encodenode test: ${test_target}")
endforeach()
