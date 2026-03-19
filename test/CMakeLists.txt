# test/CMakeLists.txt
cmake_minimum_required(VERSION 3.18)

# 添加测试源文件
file(GLOB_RECURSE TEST_SOURCES "*.cpp" "*/test/*.cpp")

# 创建测试可执行文件
foreach(test_file ${TEST_SOURCES})
    get_filename_component(test_name ${test_file} NAME_WE)
    set(test_target_name "test_${test_name}")
    
    add_executable(${test_target_name} ${test_file} "rtsp/rtsp.cpp")
    target_include_directories(${test_target_name} PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(${test_target_name} 
        PRIVATE 
            myapp_lib
            spdlog::spdlog
            Boost::asio
            Boost::system
            Boost::json
            Boost::process
            Boost::pool
            async_simple::async_simple
            yaml-cpp::yaml-cpp

    )
    
    # 添加测试
    add_test(NAME ${test_target_name} COMMAND ${test_target_name})
endforeach()

# 也可以创建一个总的测试目标
add_custom_target(test_all COMMAND ${CMAKE_CTEST_COMMAND} --verbose)