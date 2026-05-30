# modules/media/defines/test/CMakeLists.txt

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY /bin)

file(GLOB DEFINES_TEST_SOURCES "*.cpp")

foreach(test_file )
    get_filename_component(test_name  NAME_WE)
    set(test_target "test_defines_")

    add_executable( )

    target_include_directories( PRIVATE
        /../include
    )

    target_link_libraries(
        PRIVATE
            media_defines_lib
            common_lib
    )

    add_test(NAME  COMMAND )
    message(STATUS "Added defines test: ")
endforeach()