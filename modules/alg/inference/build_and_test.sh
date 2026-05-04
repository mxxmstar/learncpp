#!/bin/bash
# ========================================
# Inference 模块编译和测试脚本 (Linux/Mac)
# ========================================

echo "========================================"
echo "Building Inference Module"
echo "========================================"
echo ""

# 设置构建目录
BUILD_DIR="build"
mkdir -p $BUILD_DIR

# 进入构建目录
cd $BUILD_DIR

# 配置 CMake
echo "[1/3] Configuring CMake..."
cmake .. -DBUILD_ALG_TESTS=ON \
         -DCMAKE_BUILD_TYPE=Release || exit 1

# 编译
echo ""
echo "[2/3] Building..."
cmake --build . --target alg_inference -j$(nproc) || exit 1

# 编译示例程序
echo ""
echo "[3/3] Building examples..."
cmake --build . --target inference_example -j$(nproc) || exit 1

echo ""
echo "========================================"
echo "Build completed successfully!"
echo "========================================"
echo ""
echo "To run tests:"
echo "  cd test"
echo "  ./test_inference"
echo ""
echo "To run examples:"
echo "  cd bin"
echo "  ./inference_example"
echo ""
