# test/CMakeLists.txt
cmake_minimum_required(VERSION 3.18)

# 选项：控制是否编译特定测试
option(BUILD_SQLITE_TESTS "Build SQLite tests" ON)
option(BUILD_NET_TESTS "Build Net tests" ON)
option(BUILD_ZLM_TESTS "Build ZLM tests" ON)
option(BUILD_LOG_TESTS "Build Log tests" OFF)
option(BUILD_CONFIG_TESTS "Build Config tests" OFF)
option(BUILD_FFMPEG_TESTS "Build FFmpeg tests" OFF)
option(BUILD_COMMON_TESTS "Build Common tests" OFF)
option(BUILD_BOOST_TESTS "Build Boost tests" OFF)
option(BUILD_ASYNC_SIMPLE_TESTS "Build Async Simple tests" OFF)
option(BUILD_SERVICE_TESTS "Build Service tests" ON)

# 辅助函数：为目录中的每个 cpp 文件创建独立的测试目标
function(create_test_from_dir dir_name)
    file(GLOB TEST_SOURCES "${dir_name}/*.cpp")
    if(TEST_SOURCES)
        get_filename_component(test_dir_name ${dir_name} NAME)
        
        # 为每个测试文件创建独立的可执行文件
        foreach(test_file ${TEST_SOURCES})
            get_filename_component(test_file_name ${test_file} NAME_WE)
            set(test_target_name "test_${test_dir_name}_${test_file_name}")
            
            add_executable(${test_target_name} ${test_file})
            target_include_directories(${test_target_name} PRIVATE 
                ${CMAKE_SOURCE_DIR}/include 
                ${FFMPEG_INCLUDE_DIRS}
            )
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
                    ${FFMPEG_LIBRARIES}
            )
            
            # 如果是 Windows 且提供了 SQLite3 库路径，则使用它
            if(WIN32 AND DEFINED SQLITE3_LIB_PATH_TEST)
                list(APPEND TEST_LINK_LIBS "${SQLITE3_LIB_PATH_TEST}")
            else()
                list(APPEND TEST_LINK_LIBS sqlite3)
            endif()
            
            target_link_libraries(${test_target_name} 
                PRIVATE 
                    ${TEST_LINK_LIBS}
            )
            
            # 添加测试
            add_test(NAME ${test_target_name} COMMAND ${test_target_name})
            
            message(STATUS "Added test: ${test_target_name}")
        endforeach()
    endif()
endfunction()

# 根据选项添加对应的测试
if(BUILD_SQLITE_TESTS)
    create_test_from_dir("${CMAKE_CURRENT_LIST_DIR}/sqlite")
endif()

if(BUILD_NET_TESTS)
    create_test_from_dir("${CMAKE_CURRENT_LIST_DIR}/net")
endif()

if(BUILD_ZLM_TESTS)
    create_test_from_dir("${CMAKE_CURRENT_LIST_DIR}/zlm")
endif()

if(BUILD_LOG_TESTS)
    create_test_from_dir("${CMAKE_CURRENT_LIST_DIR}/log")
endif()

if(BUILD_CONFIG_TESTS)
    create_test_from_dir("${CMAKE_CURRENT_LIST_DIR}/config")
endif()

if(BUILD_FFMPEG_TESTS)
    create_test_from_dir("${CMAKE_CURRENT_LIST_DIR}/ffmpeg")
endif()

if(BUILD_COMMON_TESTS)
    create_test_from_dir("${CMAKE_CURRENT_LIST_DIR}/common")
endif()

if(BUILD_BOOST_TESTS)
    create_test_from_dir("${CMAKE_CURRENT_LIST_DIR}/boost")
endif()

if(BUILD_ASYNC_SIMPLE_TESTS)
    create_test_from_dir("${CMAKE_CURRENT_LIST_DIR}/async_simple")
endif()

if(BUILD_SERVICE_TESTS)
    # service 目录有独立的 CMakeLists.txt，直接 add_subdirectory
    add_subdirectory(service)
endif()

# 如果没有任何测试被启用，则默认构建所有测试（向后兼容）
if(NOT (BUILD_SQLITE_TESTS OR BUILD_NET_TESTS OR BUILD_ZLM_TESTS OR 
        BUILD_LOG_TESTS OR BUILD_CONFIG_TESTS OR BUILD_FFMPEG_TESTS OR 
        BUILD_COMMON_TESTS OR BUILD_BOOST_TESTS OR BUILD_ASYNC_SIMPLE_TESTS OR BUILD_SERVICE_TESTS))
    
    message(STATUS "No specific test type enabled, building all tests...")
    
    # 获取所有子目录
    file(GLOB sub_dirs RELATIVE ${CMAKE_CURRENT_LIST_DIR} *)
    
    foreach(dir ${sub_dirs})
        if(IS_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/${dir})
            create_test_from_dir("${CMAKE_CURRENT_LIST_DIR}/${dir}")
        endif()
    endforeach()
endif()

# 创建总的测试目标
add_custom_target(test_all COMMAND ${CMAKE_CTEST_COMMAND} --verbose)