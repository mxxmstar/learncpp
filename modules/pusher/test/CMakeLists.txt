cmake_minimum_required(VERSION 3.15)

file(GLOB TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

foreach(source ${TEST_SOURCES})
    get_filename_component(test_name ${source} NAME_WE)
    
    add_executable(test_pusher_${test_name} ${source})
    
    target_link_libraries(test_pusher_${test_name} PRIVATE
        pusher_lib
        common_lib
        ffmpeg_opt_lib
        opencv_world
    )
    
    target_include_directories(test_pusher_${test_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/modules/common/include
        ${CMAKE_SOURCE_DIR}/modules/ffmpeg_opt/include
        ${CMAKE_SOURCE_DIR}/modules/pusher/include
    )
    
    set_target_properties(test_pusher_${test_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    )
    
    if(WIN32)
        target_compile_options(test_pusher_${test_name} PRIVATE /utf-8)
    endif()
endforeach()