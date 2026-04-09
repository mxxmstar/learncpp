# gRPC 模块快速开始指南

## 📋 前置条件

确保 vcpkg.json 中已包含 gRPC 和 protobuf：

```json
{
  "dependencies": [
    "grpc",
    "protobuf"
  ]
}
```

## 🚀 快速开始

### 1. 安装依赖

```bash
cd d:\file_mx\aaaaa\learncpp
vcpkg install
```

### 2. 重新配置 CMake

```bash
cd out/build/x64-Debug
cmake ../..
cmake --build . --config Debug
```

### 3. 生成 gRPC 代码

CMake 会自动生成 gRPC 代码。如果需要手动生成：

```powershell
# PowerShell
$PROTOC = "out/build/x64-Debug/vcpkg_installed/x64-windows/tools/protobuf/protoc.exe"
$PLUGIN = "out/build/x64-Debug/vcpkg_installed/x64-windows/tools/grpc/grpc_cpp_plugin.exe"

& $PROTOC `
  --proto_path=grpc/proto `
  --cpp_out=build/grpc_generated_temp `
  --grpc_out=build/grpc_generated_temp `
  --plugin=protoc-gen-grpc=$PLUGIN `
  grpc/proto/hello.proto

# 然后手动分离文件：
# .h 文件 -> include/grpc/generated/
# .cc 文件 -> src/grpc/generated/
```

### 4. 编译测试

```bash
cmake --build . --config Debug --target test_grpc_hello
```

### 5. 运行测试

```bash
cd d:\file_mx\aaaaa\learncpp
.\bin\test_grpc_hello.exe
```

## 📁 创建的文件

### 核心文件

```
learncpp/
├── cmake/
│   └── GrpcUtils.cmake              # gRPC CMake 工具函数
├── include/grpc/
│   ├── grpc_server.h                # gRPC 服务端基类
│   ├── grpc_client.h                # gRPC 客户端基类
│   ├── hello_grpc_service.h         # Hello 服务定义
│   └── generated/                   # 自动生成的头文件
│       ├── hello.pb.h
│       └── hello.grpc.pb.h
├── src/grpc/
│   ├── grpc_server.cpp              # gRPC 服务端实现
│   ├── grpc_client.cpp              # gRPC 客户端实现
│   ├── hello_grpc_service.cpp       # Hello 服务实现
│   └── generated/                   # 自动生成的源文件
│       ├── hello.pb.cc
│       └── hello.grpc.pb.cc
├── grpc/proto/
│   └── hello.proto                  # Proto 定义文件
└── test/service/
    └── test_grpc_hello.cpp          # gRPC 测试
```

### 修改的文件

- `CMakeLists.txt` - 添加 gRPC 依赖和链接
- `test/service/CMakeLists.txt` - 添加 gRPC 测试目标

## 🧪 测试说明

运行 `test_grpc_hello.exe` 会执行以下测试：

1. **Server Lifecycle** - 测试服务器启动/停止
2. **Client Connection** - 测试客户端连接
3. **Unary RPC** - 测试 SayHello（简单请求响应）
4. **Server Streaming** - 测试 SayHelloStream（流式响应）
5. **Concurrent Clients** - 测试多客户端并发

## 🔧 常见问题

### Q: protoc 或 grpc_cpp_plugin 找不到？

A: 确保 vcpkg 已安装 gRPC：

```bash
vcpkg install grpc protobuf
```

然后重新配置 CMake。

### Q: 编译时出现链接错误？

A: 检查 CMakeLists.txt 中是否已添加：

```cmake
find_package(gRPC CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)

target_link_libraries(your_target PRIVATE
    gRPC::grpc++
    gRPC::grpc++_reflection
    protobuf::libprotobuf
)
```

### Q: 生成的文件在哪里？

A: 
- 头文件（`.h`）：`include/grpc/generated/`
- 源文件（`.cc`）：`src/grpc/generated/`

### Q: 如何添加新的 proto 文件？

A: 
1. 在 `grpc/proto/` 中创建 `.proto` 文件
2. 在 `test/service/CMakeLists.txt` 的 `grpc_generate_cpp` 中添加新文件
3. 重新运行 CMake

## 📝 下一步

现在你有了：
- ✅ gRPC 服务端和客户端基类
- ✅ Hello 服务示例（Unary + Streaming）
- ✅ 完整的测试用例
- ✅ CMake 自动构建配置

接下来可以：
1. 查看 `test_grpc_hello.cpp` 了解如何使用
2. 创建你自己的 proto 文件
3. 实现 YOLOv5 检测服务
4. 集成到视频流水线

## 📚 参考文档

- 详细文档：`docs/GRPC_MODULE.md`
- gRPC C++ 教程：https://grpc.io/docs/languages/cpp/
- Protocol Buffers：https://protobuf.dev/
