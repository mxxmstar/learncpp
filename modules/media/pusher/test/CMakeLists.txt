cmake_minimum_required(VERSION 3.15)

file(GLOB TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

foreach(source ${TEST_SOURCES})
    get_filename_component(test_name ${source} NAME_WE)
    add_executable(test_media_pusher_${test_name} ${source})

    target_link_libraries(test_media_pusher_${test_name}
        PRIVATE
            media_pusher_lib
            common_lib
    )

    target_include_directories(test_media_pusher_${test_name}
        PRIVATE
            ${CMAKE_SOURCE_DIR}/modules/media/pusher/include
            ${CMAKE_SOURCE_DIR}/modules/media/defines/include
            ${CMAKE_SOURCE_DIR}/modules/common/include
    )

    if(WIN32)
        target_compile_options(test_media_pusher_${test_name} PRIVATE /utf-8)
    endif()
endforeach()
