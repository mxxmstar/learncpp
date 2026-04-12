# GRPC 模块测试文件迁移完成

## 概述

已将 grpc 模块的测试文件从 `test/grpc/` 迁移到 `modules/grpc/test/`，符合模块化架构规范。

---

## 迁移的文件

### 测试源代码

1. ✅ `test_video_grpc_client.cpp` - VideoGrpcClient 功能测试
2. ✅ `test_grpc_algorithm_integration.cpp` - gRPC 算法集成测试
3. ✅ `test_video_processor.cpp` - 视频处理器测试

### Python 测试脚本

4. ✅ `test_grpc_python.py` - Python gRPC 客户端测试
5. ✅ `test_video_server.py` - Python 视频服务器模拟

### 文档和脚本

6. ✅ `README.md` - 测试说明文档
7. ✅ `README_INTEGRATION_TEST.md` - 集成测试说明
8. ✅ `run_integration_test.ps1` - 集成测试运行脚本

---

## 目录结构

```
modules/grpc/
├── CMakeLists.txt              # 主模块配置（BUILD_GRPC_TESTS = ON）
├── include/grpc/               # 头文件
├── src/                        # 源文件
├── generated/                  # gRPC 生成的代码
├── lib/                        # 编译输出
└── test/                       # ← 新增测试目录
    ├── CMakeLists.txt          # 测试配置
    ├── test_video_grpc_client.cpp
    ├── test_grpc_algorithm_integration.cpp
    ├── test_video_processor.cpp
    ├── test_grpc_python.py
    ├── test_video_server.py
    ├── README.md
    ├── README_INTEGRATION_TEST.md
    └── run_integration_test.ps1
```

---

## 测试配置

### `modules/grpc/test/CMakeLists.txt`

配置了 3 个测试可执行文件：

**1. test_grpc_video_client**
- 测试文件：`test_video_grpc_client.cpp`
- 依赖：grpc_lib, log_lib
- 用途：测试 VideoGrpcClient 的基本功能

**2. test_grpc_algorithm_integration**
- 测试文件：`test_grpc_algorithm_integration.cpp`
- 依赖：grpc_lib, alg_lib, log_lib
- 用途：测试 gRPC 与算法模块的集成

**3. test_grpc_video_processor**（条件编译）
- 测试文件：`test_video_processor.cpp`
- 依赖：grpc_lib, log_lib
- 用途：测试视频处理器功能

---

## 启用测试

### 方法 1：在模块 CMakeLists.txt 中启用（已设置）

```cmake
# modules/grpc/CMakeLists.txt
option(BUILD_GRPC_TESTS "Build grpc module tests" ON)  # ← 已设置为 ON
```

### 方法 2：在命令行中启用

```powershell
cmake -DBUILD_GRPC_TESTS=ON ..
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
[xx/xx] Building CXX object modules/grpc/test/CMakeFiles/test_grpc_video_client.dir/test_video_grpc_client.cpp.obj
[xx/xx] Building CXX object modules/grpc/test/CMakeFiles/test_grpc_algorithm_integration.dir/test_grpc_algorithm_integration.cpp.obj
[xx/xx] Linking CXX executable K:\...\modules\grpc\test\bin\test_grpc_video_client.exe
[xx/xx] Linking CXX executable K:\...\modules\grpc\test\bin\test_grpc_algorithm_integration.exe
```

测试可执行文件会输出到：
- `modules/grpc/test/bin/test_grpc_video_client.exe`
- `modules/grpc/test/bin/test_grpc_algorithm_integration.exe`

---

## 运行测试

### 方法 1：直接运行可执行文件

```powershell
cd modules\grpc\test\bin
.\test_grpc_video_client.exe
.\test_grpc_algorithm_integration.exe
```

### 方法 2：使用 CTest

```powershell
cd out\build\x64-debug
ctest -R grpc --verbose
```

### 方法 3：运行集成测试

```powershell
cd modules\grpc\test
.\run_integration_test.ps1
```

这会启动 Python 服务器并运行完整的集成测试。

---

## 测试说明

### test_video_grpc_client.cpp

测试 VideoGrpcClient 的核心功能：
- 连接到 gRPC 服务器
- 发送视频帧
- 接收处理结果
- 错误处理

### test_grpc_algorithm_integration.cpp

测试算法模块与 gRPC 的集成：
- GrpcToAlg 处理器
- 视频帧通过 gRPC 发送到 Python 算法服务
- 接收检测结果

### Python 测试脚本

- `test_grpc_python.py` - Python 客户端示例
- `test_video_server.py` - 模拟 Python 算法服务器

用于手动测试和调试 gRPC 通信。

---

## 注意事项

### 1. 依赖关系

测试需要以下模块已编译：
- ✅ grpc_lib
- ✅ alg_lib（用于集成测试）
- ✅ log_lib

### 2. Python 环境

运行 Python 测试脚本需要：
```bash
pip install grpcio grpcio-tools protobuf opencv-python
```

### 3. 服务器地址

测试默认连接到 `localhost:50051`，确保 gRPC 服务器正在运行。

### 4. 原始测试目录

原始的 `test/grpc/` 目录仍然保留，可以安全删除：

```powershell
# 确认新测试工作正常后
Remove-Item -Path "test\grpc" -Recurse -Force
```

---

## 状态

✅ **测试文件已迁移到 modules/grpc/test/**
✅ **CMakeLists.txt 已配置**
✅ **BUILD_GRPC_TESTS 已启用**
✅ **3 个测试目标已定义**

请在 Visual Studio 中重新配置并编译项目！🎉
