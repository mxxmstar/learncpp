#!/bin/bash

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 默认参数
BUILD_TYPE="Release"
BUILD_TESTS="ON"
CLEAN_BUILD=false
NUM_JOBS=$(nproc)

# 打印帮助信息
print_help() {
    echo "Usage: $0 [OPTIONS]"
    echo "Build script for MySelfContainedApp"
    echo ""
    echo "Options:"
    echo "  -t, --type TYPE       Build type: Debug|Release|RelWithDebInfo|MinSizeRel (default: Release)"
    echo "  --test [ON|OFF]       Build tests: ON|OFF (default: ON)"
    echo "  -j, --jobs NUM        Number of parallel jobs (default: number of CPU cores)"
    echo "  -c, --clean           Clean previous build and rebuild"
    echo "  -h, --help            Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0                              # Build in Release mode with tests"
    echo "  $0 -t Debug                     # Build in Debug mode"
    echo "  $0 -c                           # Clean and rebuild"
    echo "  $0 --test OFF                   # Build without tests"
}

# 解析命令行参数
while [[ $# -gt 0 ]]; do
    case $1 in
        -t|--type)
            BUILD_TYPE="$2"
            shift 2
            ;;
        --test)
            BUILD_TESTS="$2"
            shift 2
            ;;
        -j|--jobs)
            NUM_JOBS="$2"
            shift 2
            ;;
        -c|--clean)
            CLEAN_BUILD=true
            shift
            ;;
        -h|--help)
            print_help
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            print_help
            exit 1
            ;;
    esac
done

echo -e "${GREEN}Build Configuration:${NC}"
echo -e "  Build Type: $BUILD_TYPE"
echo -e "  Build Tests: $BUILD_TESTS"
echo -e "  Parallel Jobs: $NUM_JOBS"
echo -e "  Clean Build: $CLEAN_BUILD"
echo ""

# 检查必要工具
if ! command -v cmake &> /dev/null; then
    echo -e "${RED}Error: cmake is not installed${NC}"
    exit 1
fi

if ! command -v make &> /dev/null; then
    echo -e "${RED}Error: make is not installed${NC}"
    exit 1
fi

# 创建构建目录
BUILD_DIR="build"
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${GREEN}Creating build directory...${NC}"
    mkdir -p "$BUILD_DIR"
fi

cd "$BUILD_DIR"

# 如果是清理构建，则删除旧的构建文件
if [ "$CLEAN_BUILD" = true ]; then
    echo -e "${GREEN}Cleaning previous build...${NC}"
    rm -rf ./*
fi

# 配置项目（如果CMakeFiles不存在或执行清理构建）
if [ ! -f "Makefile" ] || [ ! -d "CMakeFiles" ] || [ "$CLEAN_BUILD" = true ]; then
    echo -e "${GREEN}Configuring project...${NC}"
    cmake .. \
        -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
        -DBUILD_TESTS=$BUILD_TESTS \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    
    if [ $? -ne 0 ]; then
        echo -e "${RED}CMake configuration failed${NC}"
        exit 1
    fi
else
    echo -e "${YELLOW}Using existing build configuration${NC}"
fi

# 编译项目
echo -e "${GREEN}Building project with $NUM_JOBS parallel jobs...${NC}"
make -j"$NUM_JOBS"

if [ $? -eq 0 ]; then
    echo -e "${GREEN}Build completed successfully!${NC}"
    
    # 显示构建的可执行文件位置
    if [ -f "../bin/MySelfContainedApp" ]; then
        echo -e "${GREEN}Executable: ../bin/MySelfContainedApp${NC}"
    fi
    
    # 如果构建了测试，显示测试可执行文件
    if [ "$BUILD_TESTS" = "ON" ]; then
        echo -e "${GREEN}Looking for test executables...${NC}"
        TEST_EXECS=$(find . -name "test_*" -type f -executable 2>/dev/null)
        if [ -n "$TEST_EXECS" ]; then
            echo -e "${GREEN}Test executables found:${NC}"
            for test_exec in $TEST_EXECS; do
                echo -e "  ${GREEN}- $test_exec${NC}"
            done
        fi
    fi
else
    echo -e "${RED}Build failed${NC}"
    exit 1
fi

# 如果在Debug模式下构建，生成compile_commands.json链接
if [ "$BUILD_TYPE" = "Debug" ] && [ -f "compile_commands.json" ]; then
    ln -sf "$PWD/compile_commands.json" ../compile_commands.json
    echo -e "${GREEN}Created symlink to compile_commands.json${NC}"
fi

cd ..