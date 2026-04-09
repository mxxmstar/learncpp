# CMake 函数：从 .proto 文件生成 C++ gRPC 代码
# 用法1: grpc_generate_cpp(OUTPUT_FILES INPUT_PROTO_DIR OUTPUT_DIR PROTO_FILES...)
# 用法2: grpc_generate_cpp_separated(HEADER_FILES SOURCE_FILES INPUT_PROTO_DIR HEADER_DIR SOURCE_DIR PROTO_FILES...)
function(grpc_generate_cpp OUTPUTS INPUT_PROTO_DIR OUTPUT_DIR)
  # 查找 protoc 和 grpc_cpp_plugin
  find_program(PROTOC protoc PATHS ${CMAKE_PREFIX_PATH} NO_DEFAULT_PATH)
  if(NOT PROTOC)
    find_program(PROTOC protoc)
  endif()
  
  find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin PATHS ${CMAKE_PREFIX_PATH} NO_DEFAULT_PATH)
  if(NOT GRPC_CPP_PLUGIN)
    find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin)
  endif()
  
  if(NOT PROTOC)
    message(FATAL_ERROR "protoc not found. Please ensure protobuf is installed via vcpkg.")
  endif()
  
  if(NOT GRPC_CPP_PLUGIN)
    message(FATAL_ERROR "grpc_cpp_plugin not found. Please ensure gRPC is installed via vcpkg.")
  endif()
  
  message(STATUS "Found protoc: ${PROTOC}")
  message(STATUS "Found grpc_cpp_plugin: ${GRPC_CPP_PLUGIN}")
  
  set(${OUTPUTS} "" PARENT_SCOPE)
  
  foreach(PROTO_FILE ${ARGN})
    get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
    get_filename_component(PROTO_PATH ${PROTO_FILE} DIRECTORY)
    
    # 计算输出文件路径
    set(PB_H "${OUTPUT_DIR}/${PROTO_NAME}.pb.h")
    set(PB_CC "${OUTPUT_DIR}/${PROTO_NAME}.pb.cc")
    set(GRPC_PB_H "${OUTPUT_DIR}/${PROTO_NAME}.grpc.pb.h")
    set(GRPC_PB_CC "${OUTPUT_DIR}/${PROTO_NAME}.grpc.pb.cc")
    
    # 添加自定义命令
    add_custom_command(
      OUTPUT ${PB_H} ${PB_CC} ${GRPC_PB_H} ${GRPC_PB_CC}
      COMMAND ${PROTOC}
      ARGS --proto_path=${INPUT_PROTO_DIR}
           --cpp_out=${OUTPUT_DIR}
           --grpc_out=${OUTPUT_DIR}
           --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
           ${PROTO_FILE}
      DEPENDS ${PROTO_FILE}
      COMMENT "Generating gRPC code from ${PROTO_FILE}"
      VERBATIM
    )
    
    list(APPEND ${OUTPUTS} ${PB_H} ${PB_CC} ${GRPC_PB_H} ${GRPC_PB_CC})
  endforeach()
  
  set(${OUTPUTS} ${${OUTPUTS}} PARENT_SCOPE)
endfunction()

# 增强版：将头文件和源文件分别输出到不同目录
function(grpc_generate_cpp_separated HEADER_OUTPUTS SOURCE_OUTPUTS INPUT_PROTO_DIR HEADER_DIR SOURCE_DIR)
  # 查找 protoc 和 grpc_cpp_plugin
  find_program(PROTOC protoc PATHS ${CMAKE_PREFIX_PATH} NO_DEFAULT_PATH)
  if(NOT PROTOC)
    find_program(PROTOC protoc)
  endif()
  
  find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin PATHS ${CMAKE_PREFIX_PATH} NO_DEFAULT_PATH)
  if(NOT GRPC_CPP_PLUGIN)
    find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin)
  endif()
  
  if(NOT PROTOC)
    message(FATAL_ERROR "protoc not found. Please ensure protobuf is installed via vcpkg.")
  endif()
  
  if(NOT GRPC_CPP_PLUGIN)
    message(FATAL_ERROR "grpc_cpp_plugin not found. Please ensure gRPC is installed via vcpkg.")
  endif()
  
  message(STATUS "Found protoc: ${PROTOC}")
  message(STATUS "Found grpc_cpp_plugin: ${GRPC_CPP_PLUGIN}")
  message(STATUS "Header output dir: ${HEADER_DIR}")
  message(STATUS "Source output dir: ${SOURCE_DIR}")
  
  set(${HEADER_OUTPUTS} "" PARENT_SCOPE)
  set(${SOURCE_OUTPUTS} "" PARENT_SCOPE)
  
  foreach(PROTO_FILE ${ARGN})
    get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
    
    # 头文件输出到头文件目录
    set(PB_H "${HEADER_DIR}/${PROTO_NAME}.pb.h")
    set(GRPC_PB_H "${HEADER_DIR}/${PROTO_NAME}.grpc.pb.h")
    
    # 源文件输出到源文件目录
    set(PB_CC "${SOURCE_DIR}/${PROTO_NAME}.pb.cc")
    set(GRPC_PB_CC "${SOURCE_DIR}/${PROTO_NAME}.grpc.pb.cc")
    
    # 添加自定义命令（先生成到临时目录，然后复制）
    set(TEMP_DIR "${CMAKE_BINARY_DIR}/grpc_generated_temp")
    file(MAKE_DIRECTORY ${TEMP_DIR})
    
    add_custom_command(
      OUTPUT ${PB_H} ${PB_CC} ${GRPC_PB_H} ${GRPC_PB_CC}
      COMMAND ${PROTOC}
      ARGS --proto_path=${INPUT_PROTO_DIR}
           --cpp_out=${TEMP_DIR}
           --grpc_out=${TEMP_DIR}
           --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
           ${PROTO_FILE}
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              "${TEMP_DIR}/${PROTO_NAME}.pb.h" "${HEADER_DIR}/${PROTO_NAME}.pb.h"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              "${TEMP_DIR}/${PROTO_NAME}.grpc.pb.h" "${HEADER_DIR}/${PROTO_NAME}.grpc.pb.h"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              "${TEMP_DIR}/${PROTO_NAME}.pb.cc" "${SOURCE_DIR}/${PROTO_NAME}.pb.cc"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              "${TEMP_DIR}/${PROTO_NAME}.grpc.pb.cc" "${SOURCE_DIR}/${PROTO_NAME}.grpc.pb.cc"
      DEPENDS ${PROTO_FILE}
      COMMENT "Generating gRPC code from ${PROTO_FILE} (separated headers and sources)"
      VERBATIM
    )
    
    list(APPEND ${HEADER_OUTPUTS} ${PB_H} ${GRPC_PB_H})
    list(APPEND ${SOURCE_OUTPUTS} ${PB_CC} ${GRPC_PB_CC})
  endforeach()
  
  set(${HEADER_OUTPUTS} ${${HEADER_OUTPUTS}} PARENT_SCOPE)
  set(${SOURCE_OUTPUTS} ${${SOURCE_OUTPUTS}} PARENT_SCOPE)
endfunction()
