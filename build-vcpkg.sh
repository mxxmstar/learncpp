#!/bin/bash

# 清理之前的构建目录
rm -rf build/

# 运行 cmake 配置，使用 vcpkg 工具链
cmake -B build -S . \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/vcpkg/scripts/buildsystems/vcpkg.cmake"

echo "CMake configuration with vcpkg completed."