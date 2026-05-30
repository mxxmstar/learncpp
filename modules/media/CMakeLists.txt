# modules/media/CMakeLists.txt
# Media 模块顶层：组织子模块。
cmake_minimum_required(VERSION 3.18)
project(media_module LANGUAGES C CXX)

add_subdirectory(defines)
add_subdirectory(puller)
add_subdirectory(decoder)