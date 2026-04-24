# modules/config/test/CMakeLists.txt

# 设置测试可执行文件输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

# 收集测试文件
file(GLOB CONFIG_TEST_SOURCES "*.cpp")

foreach(test_file ${CONFIG_TEST_SOURCES})
    get_filename_component(test_name ${test_file} NAME_WE)
    set(test_target "test_config_${test_name}")
    
    add_executable(${test_target} ${test_file})
    
    target_include_directories(${test_target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    
    target_link_libraries(${test_target}
        PRIVATE
            common_lib
    )
    
    # 添加测试
    add_test(NAME ${test_target} COMMAND ${test_target})
    
    message(STATUS "Added config test: ${test_target}")
endforeach()
