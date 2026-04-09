# gRPC 模块封装

## 概述

本模块提供了简单的 gRPC 服务端和客户端封装，用于后续扩展 YOLOv5 检测服务。

## 目录结构

```
learncpp/
├── include/grpc/
│   ├── grpc_server.h              # gRPC 服务端基类
│   ├── grpc_client.h              # gRPC 客户端基类
│   ├── hello_grpc_service.h       # Hello 服务示例
│   └── generated/                 # 自动生成的头文件
│       ├── hello.pb.h
│       └── hello.grpc.pb.h
├── src/grpc/
│   ├── grpc_server.cpp
│   ├── grpc_client.cpp
│   ├── hello_grpc_service.cpp
│   └── generated/                 # 自动生成的源文件
│       ├── hello.pb.cc
│       └── hello.grpc.pb.cc
├── grpc/proto/
│   └── hello.proto                # Protocol Buffer 定义
└── test/service/
    └── test_grpc_hello.cpp        # 测试文件
```

## 功能特性

### 1. GrpcServer 基类

提供 gRPC 服务端的统一生命周期管理：

```cpp
class GrpcServer {
public:
    explicit GrpcServer(const std::string& address);
    bool Initialize();      // 初始化服务器
    bool Start();           // 启动服务器（后台线程）
    void Stop();            // 停止服务器
    void Wait();            // 等待服务器退出
    bool IsRunning();       // 是否正在运行

protected:
    virtual void RegisterServices(grpc::ServerBuilder& builder) = 0;
};
```

### 2. GrpcClient 基类

提供 gRPC 客户端的基础功能：

```cpp
class GrpcClient {
public:
    explicit GrpcClient(const std::string& target);
    bool WaitForConnected(int timeout_seconds);
    grpc_connectivity_state GetState(bool try_to_connect = false);
    std::shared_ptr<grpc::Channel> GetChannel();

protected:
    static std::unique_ptr<grpc::ClientContext> CreateContext(int timeout_ms = 5000);
};
```

### 3. Hello 服务示例

演示了两种 RPC 模式：

- **Unary RPC** - 简单请求响应（SayHello）
- **Server Streaming RPC** - 服务端流式响应（SayHelloStream）

## 如何生成 Proto 文件

### 1. 安装工具

需要先安装 `protoc` 和 `grpc_cpp_plugin`。使用 vcpkg 安装：

```bash
# 如果 vcpkg.json 中已经包含 grpc 和 protobuf，运行：
cd path/to/learncpp
vcpkg install
```

### 2. 生成 C++ 代码

Windows PowerShell：

```powershell
# 进入项目根目录
cd d:\file_mx\aaaaa\learncpp

# 生成 .pb.cc 和 .pb.h（消息类）
$PROTOC_EXE = "out/build/x64-Debug/vcpkg_installed/x64-windows/tools/protobuf/protoc.exe"
$PLUGIN_EXE = "out/build/x64-Debug/vcpkg_installed/x64-windows/tools/grpc/grpc_cpp_plugin.exe"

& $PROTOC_EXE `
  --proto_path=service/proto `
  --cpp_out=include/service/generated `
  --grpc_out=include/service/generated `
  --plugin=protoc-gen-grpc=$PLUGIN_EXE `
  service/proto/hello.proto
```

Linux/Mac：

```bash
cd path/to/learncpp

PROTOC="out/build/x64-Debug/vcpkg_installed/x64-linux/tools/protobuf/protoc"
PLUGIN="out/build/x64-Debug/vcpkg_installed/x64-linux/tools/grpc/grpc_cpp_plugin"

$PROTOC \
  --proto_path=service/proto \
  --cpp_out=include/service/generated \
  --grpc_out=include/service/generated \
  --plugin=protoc-gen-grpc=$PLUGIN \
  service/proto/hello.proto
```

### 3. 生成的文件

生成后会创建以下文件到 `include/grpc/generated/`：

- `hello.pb.h` - Protobuf 消息头文件
- `hello.pb.cc` - Protobuf 消息实现
- `hello.grpc.pb.h` - gRPC 服务头文件
- `hello.grpc.pb.cc` - gRPC 服务实现

## 编译和运行测试

### 1. 更新 CMakeLists.txt

需要在 `CMakeLists.txt` 中添加 gRPC 和 protobuf 依赖：

```cmake
# 查找 gRPC 和 Protobuf
find_package(gRPC CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)

# 链接到 myapp_lib
target_link_libraries(myapp_lib
    PUBLIC
        gRPC::grpc++
        gRPC::grpc++_reflection
        protobuf::libprotobuf
)
```

### 2. 配置 CMake

```bash
cd out/build/x64-Debug
cmake ../..
cmake --build . --config Debug
```

### 3. 运行测试

```bash
# 运行 gRPC 测试
./bin/test_grpc_hello.exe
```

## 测试用例说明

测试文件 `test_grpc_hello.cpp` 包含以下测试：

1. **Test 1: Server Lifecycle** - 测试服务器启动和停止
2. **Test 2: Client Connection** - 测试客户端连接
3. **Test 3: Unary RPC** - 测试 SayHello 方法
4. **Test 4: Server Streaming** - 测试 SayHelloStream 方法
5. **Test 5: Concurrent Clients** - 测试多个客户端并发请求

## 如何扩展为 YOLOv5 服务

### 1. 定义 Proto 文件

创建 `service/proto/yolo_detection.proto`：

```protobuf
syntax = "proto3";

package yolo_detection;

message ImageData {
  bytes data = 1;        // 图像数据（JPEG/PNG 编码）
  int32 width = 2;
  int32 height = 3;
}

message DetectionResult {
  int32 class_id = 1;
  string class_name = 2;
  float confidence = 3;
  float x1 = 4;
  float y1 = 5;
  float x2 = 6;
  float y2 = 7;
}

message DetectionRequest {
  int32 channel_id = 1;
  ImageData image = 2;
  float confidence_threshold = 3;
}

message DetectionResponse {
  int32 channel_id = 1;
  int64 timestamp_us = 2;
  repeated DetectionResult detections = 3;
}

service YoloV5DetectionService {
  rpc Detect (DetectionRequest) returns (DetectionResponse);
  rpc StreamDetect (stream DetectionRequest) returns (stream DetectionResponse);
}
```

### 2. 实现服务类

参考 `HelloGrpcServer` 和 `HelloGrpcClient`，创建 `YoloGrpcServer` 和 `YoloGrpcClient`：

```cpp
class YoloGrpcServer : public GrpcServer {
public:
    explicit YoloGrpcServer(const std::string& address);
    
protected:
    void RegisterServices(grpc::ServerBuilder& builder) override;

private:
    YoloDetectionServiceImpl service_impl_;  // 实现检测逻辑
};

class YoloGrpcClient : public GrpcClient {
public:
    explicit YoloGrpcClient(const std::string& target);
    
    bool Detect(const cv::Mat& image, int channel_id,
                std::vector<Detection>& results,
                float conf_threshold = 0.5f);
};
```

### 3. 集成到视频流水线

在视频流水线中使用 YoloGrpcClient：

```cpp
// 在 VideoPipeline 中添加
class VideoPipeline {
private:
    std::unique_ptr<YoloGrpcClient> detection_client_;
    
    void ProcessFrame(cv::Mat& frame, int64_t pts) {
        std::vector<Detection> results;
        if (detection_client_->Detect(frame, channel_id_, results)) {
            // 处理检测结果
            OnDetectionResults(channel_id_, results, pts);
        }
    }
};
```

## 常见问题

### Q: 为什么使用 InsecureChannelCredentials？

A: 当前是测试阶段，使用不安全的连接简化开发。生产环境应使用 SSL/TLS。

### Q: 如何修改超时时间？

A: 客户端方法都接受 `timeout_ms` 参数，默认 5000ms。

### Q: 如何处理大图像数据？

A: 在创建 Channel 时设置 `SetMaxReceiveMessageSize` 和 `SetMaxSendMessageSize`。

## 下一步

- [ ] 生成 proto 文件
- [ ] 实现 YOLOv5 检测服务
- [ ] 集成到视频流水线
- [ ] 添加 SSL/TLS 支持
- [ ] 添加错误处理和重试机制
- [ ] 性能优化和压力测试
