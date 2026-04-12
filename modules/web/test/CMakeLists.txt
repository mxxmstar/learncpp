# modules/web/test/CMakeLists.txt

# 设置测试可执行文件输出目录
set(TEST_OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${TEST_OUTPUT_DIR})

# 自动扫描所有测试文件
file(GLOB TEST_SOURCES "*.cpp")

# 为每个测试文件创建独立的可执行文件
foreach(test_file ${TEST_SOURCES})
    # 获取不带扩展名的文件名
    get_filename_component(test_name ${test_file} NAME_WE)
    
    # 创建可执行文件
    add_executable(test_web_${test_name} ${test_file})
    
    # 链接到 web_lib
    target_link_libraries(test_web_${test_name}
        PRIVATE
            web_lib
    )
    
    # Windows 特定配置
    if(WIN32)
        target_compile_options(test_web_${test_name} PRIVATE /utf-8)
    endif()
    
    message(STATUS "Added web test: test_web_${test_name}")
endforeach()
