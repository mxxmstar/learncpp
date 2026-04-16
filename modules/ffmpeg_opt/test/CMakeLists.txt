# FFmpeg Opt 模块测试
cmake_minimum_required(VERSION 3.15)

# 创建测试可执行文件
add_executable(test_ffmpeg_opt
    test_ffmpeg_opt.cpp
)

# 链接 ffmpeg_opt_lib
target_link_libraries(test_ffmpeg_opt PRIVATE
    ffmpeg_opt_lib
)

# 设置 C++ 标准
set_target_properties(test_ffmpeg_opt PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
