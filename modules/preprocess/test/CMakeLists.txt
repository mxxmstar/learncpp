# modules/preprocess/test/CMakeLists.txt

# 设置测试可执行文件输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

# 收集测试文件（包括子目录）
file(GLOB_RECURSE PREPROCESS_TEST_SOURCES "*.cpp")

foreach(test_file ${PREPROCESS_TEST_SOURCES})
    # 获取相对路径作为测试名称的一部分
    file(RELATIVE_PATH rel_path ${CMAKE_CURRENT_SOURCE_DIR} ${test_file})
    string(REPLACE "/" "_" test_name ${rel_path})
    string(REPLACE ".cpp" "" test_name ${test_name})
    set(test_target "test_preprocess_${test_name}")
    
    add_executable(${test_target} ${test_file})
    
    target_include_directories(${test_target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    
    target_link_libraries(${test_target}
        PRIVATE
            preprocess_lib
            decoder_lib  # preprocess 依赖 decoder
            common_lib
    )
    
    # 添加测试
    add_test(NAME ${test_target} COMMAND ${test_target})
    
    message(STATUS "Added preprocess test: ${test_target}")
endforeach()
