# VideoPipeline 模块测试文件迁移完成

## 概述

从原始的 `test/video_pipeline/` 目录复制了 3 个核心测试文件到 `modules/videopipeline/test/`，这些文件没有乱码问题。

---

## 迁移的文件

### 测试源代码（3个）

1. ✅ **test_single_channel_processing.cpp** (8.8 KB)
   - 单通道视频处理测试
   - 测试基本的视频帧处理流程

2. ✅ **test_video_pipeline.cpp** (5.1 KB)
   - 视频流水线基本功能测试
   - 测试流水线的初始化和运行

3. ✅ **test_video_pipeline_grpc.cpp** (10.9 KB)
   - 视频流水线 gRPC 集成测试
   - 测试通过 gRPC 与算法服务通信

---

## 目录结构

```
modules/videopipeline/
├── CMakeLists.txt              # 主模块配置（BUILD_VIDEOPIPELINE_TESTS = ON）
├── include/videopipeline/      # 头文件
├── src/                        # 源文件
├── lib/                        # 编译输出
└── test/                       # ← 新增测试目录
    ├── CMakeLists.txt          # 测试配置
    ├── test_single_channel_processing.cpp
    ├── test_video_pipeline.cpp
    └── test_video_pipeline_grpc.cpp
```

---

## 测试配置

### modules/videopipeline/test/CMakeLists.txt

配置了 3 个测试可执行文件：

**1. test_single_channel_processing**
- 测试文件：`test_single_channel_processing.cpp`
- 依赖：videopipeline_lib, log_lib
- 用途：测试单通道视频处理

**2. test_video_pipeline**
- 测试文件：`test_video_pipeline.cpp`
- 依赖：videopipeline_lib, log_lib
- 用途：测试视频流水线基本功能

**3. test_video_pipeline_grpc**
- 测试文件：`test_video_pipeline_grpc.cpp`
- 依赖：videopipeline_lib, alg_lib, grpc_lib, log_lib
- 用途：测试 gRPC 集成

---

## 模块配置

### modules/videopipeline/CMakeLists.txt

**测试开关：**
```cmake
option(BUILD_VIDEOPIPELINE_TESTS "Build videopipeline module tests" ON)
```

**测试子目录：**
```cmake
if(BUILD_VIDEOPIPELINE_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test")
    enable_testing()
    add_subdirectory(test)
endif()
```

**依赖关系：**
```cmake
target_link_libraries(videopipeline_lib
    PUBLIC
        ${OpenCV_LIBS}
        decoder_lib
        preprocess_lib
        postprocess_lib
        grpc_lib
        alg_lib
        puller_lib
        log_lib
        net_lib
        Boost::asio
        Boost::lockfree
)
```

---

## 编译测试

### 1. 重新配置 CMake

```
Project → Delete Cache and Reconfigure
```

### 2. 编译

```
Build → Build All
```

应该看到：

```
-- VideoPipeline module tests configured:
--   - test_single_channel_processing
--   - test_video_pipeline
--   - test_video_pipeline_grpc

[xx/xx] Building CXX object modules/videopipeline/test/CMakeFiles/test_single_channel_processing.dir/test_single_channel_processing.cpp.obj
[xx/xx] Building CXX object modules/videopipeline/test/CMakeFiles/test_video_pipeline.dir/test_video_pipeline.cpp.obj
[xx/xx] Building CXX object modules/videopipeline/test/CMakeFiles/test_video_pipeline_grpc.dir/test_video_pipeline_grpc.cpp.obj
[xx/xx] Linking CXX executable K:\...\modules\videopipeline\test\bin\test_single_channel_processing.exe
[xx/xx] Linking CXX executable K:\...\modules\videopipeline\test\bin\test_video_pipeline.exe
[xx/xx] Linking CXX executable K:\...\modules\videopipeline\test\bin\test_video_pipeline_grpc.exe
```

测试可执行文件输出到：
- `modules/videopipeline/test/bin/test_single_channel_processing.exe`
- `modules/videopipeline/test/bin/test_video_pipeline.exe`
- `modules/videopipeline/test/bin/test_video_pipeline_grpc.exe`

---

## 运行测试

### 方法 1：直接运行可执行文件

```powershell
cd modules\videopipeline\test\bin
.\test_single_channel_processing.exe
.\test_video_pipeline.exe
.\test_video_pipeline_grpc.exe
```

### 方法 2：使用 CTest

```powershell
cd out\build\x64-debug
ctest -R videopipeline --verbose
```

---

## 测试说明

### test_single_channel_processing.cpp

测试单通道视频处理流程：
- 创建视频流水线
- 配置处理器
- 处理视频帧
- 验证输出结果

### test_video_pipeline.cpp

测试视频流水线的基本功能：
- 流水线初始化
- 组件连接
- 启动和停止
- 状态管理

### test_video_pipeline_grpc.cpp

测试 gRPC 集成功能：
- 配置 gRPC 算法处理器
- 发送视频帧到 Python 服务
- 接收检测结果
- 验证通信正常

---

## 注意事项

### 1. 依赖关系

测试需要以下模块已编译：
- ✅ videopipeline_lib
- ✅ alg_lib（用于 gRPC 测试）
- ✅ grpc_lib（用于 gRPC 测试）
- ✅ log_lib
- ✅ decoder_lib, preprocess_lib, postprocess_lib（间接依赖）

### 2. 原始测试目录

原始的 `test/video_pipeline/` 目录仍然保留，包含更多测试文件：
- `test_boost_lockfree.cpp`
- `test_ffmpeg_decoder.cpp`
- `test_frame_queue.cpp`
- `test_frame_rate_controller.cpp`
- `test_multi_channel_processing.cpp`
- `test_open_cv_processor.cpp`
- `test_zlm_puller.cpp`

这些文件可以根据需要后续迁移。

### 3. 为什么选择这 3 个测试？

- ✅ **没有乱码** - 从原始目录复制，编码正确
- ✅ **核心功能** - 覆盖单通道、基本流水线、gRPC 集成
- ✅ **代表性** - 展示模块的主要用法

---

## 优势

### 1. 模块化完整

- ✅ 测试文件在模块内部
- ✅ 符合模块化架构原则
- ✅ 便于管理和维护

### 2. 清晰的依赖

- test_single_channel_processing → videopipeline_lib
- test_video_pipeline → videopipeline_lib
- test_video_pipeline_grpc → videopipeline_lib + alg_lib + grpc_lib

### 3. 自动化构建

- ✅ BUILD_VIDEOPIPELINE_TESTS 默认启用
- ✅ 自动注册到 CTest
- ✅ 测试输出到固定目录

---

## 状态

✅ **3 个测试文件已从 test/video_pipeline/ 复制**
✅ **CMakeLists.txt 已配置**
✅ **BUILD_VIDEOPIPELINE_TESTS 已启用**
✅ **测试依赖已正确设置**

请在 Visual Studio 中重新配置并编译项目！🎉
