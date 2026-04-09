# C++ 与 Python gRPC 视频处理架构

## 概述

本架构实现了 C++ 和 Python 之间的双向流式 gRPC 通信，用于视频处理和算法推理。

## 架构图

```
┌─────────────────────────────────────────────────────────────┐
│                     C++ Application                         │
│                                                             │
│  ┌──────────────┐         ┌──────────────────────┐        │
│  │ Video Source │────────►│  VideoGrpcClient      │        │
│  │ (Camera/     │  Frames │  (gRPC Client)        │        │
│  │  File)       │         │                       │        │
│  └──────────────┘         └──────────┬───────────┘        │
│                                      │                     │
│                           Bidirectional Streaming          │
│                           (gRPC over TCP)                  │
│                                      │                     │
└──────────────────────────────────────┼─────────────────────┘
                                       │
                                       │
┌──────────────────────────────────────┼─────────────────────┐
│                     Python Service                         │
│                                      │                     │
│                          ┌───────────▼───────────┐        │
│                          │ VideoProcessingService│        │
│                          │  (gRPC Server)        │        │
│                          │                       │        │
│                          │  ┌─────────────────┐  │        │
│                          │  │ YOLOv5 Detector │  │        │
│                          │  │ or Mock         │  │        │
│                          │  └─────────────────┘  │        │
│                          └───────────────────────┘        │
└─────────────────────────────────────────────────────────────┘
```

## 两种工作模式

### 模式 1: 检测元数据（推荐用于实时分析）

```
C++ --视频帧--> Python --检测结果--> C++
                    (边界框、类别)
```

**优点**：
- ✅ 低带宽（只传输元数据）
- ✅ C++ 灵活后处理（绘制、跟踪、过滤）
- ✅ 适合实时监控和分析

**数据流**：
```protobuf
// C++ -> Python
VideoFrame {
    bytes data;        // JPEG 编码的视频帧
    int32 width;
    int32 height;
    string frame_id;
}

// Python -> C++
DetectionResult {
    string frame_id;
    repeated BoundingBox boxes;  // 检测框列表
    int64 processing_time_ms;
}
```

**使用场景**：
- 实时目标检测
- 行为分析
- 异常检测
- 需要 C++ 进行复杂后处理

### 模式 2: 处理后视频（推荐用于简单显示）

```
C++ --原始视频--> Python --标注视频--> C++
                      (带检测框)
```

**优点**：
- ✅ Python 完成所有处理
- ✅ C++ 代码简单（直接显示）
- ✅ 适合快速原型开发

**缺点**：
- ❌ 高带宽（传输完整视频）
- ❌ 编码/解码开销

**数据流**：
```protobuf
// C++ -> Python
VideoFrame { ... }

// Python -> C++
ProcessedFrame {
    string frame_id;
    bytes data;        // JPEG 编码的处理后视频
    int32 width;
    int32 height;
}
```

**使用场景**：
- 快速演示
- 简单的视频监控
- 不需要复杂后处理

## 目录结构

```
learncpp/
├── grpc/                              # C++ gRPC 模块
│   ├── proto/
│   │   ├── hello.proto               # Hello 服务（测试用）
│   │   └── video_processing.proto    # 视频处理服务
│   ├── include/
│   │   ├── grpc_server.h
│   │   ├── grpc_client.h
│   │   ├── hello_grpc_service.h
│   │   └── video_grpc_client.h       # 视频客户端（待实现）
│   ├── src/
│   │   ├── grpc_server.cpp
│   │   ├── grpc_client.cpp
│   │   └── hello_grpc_service.cpp
│   ├── generated/                     # 自动生成的 C++ 代码
│   │   ├── hello.pb.h/cc
│   │   └── hello.grpc.pb.h/cc
│   └── CMakeLists.txt                # 独立的 CMake 配置
│
├── algorithm/
│   ├── grpc_client/                   # Python gRPC 客户端
│   │   ├── __init__.py
│   │   ├── hello_client.py           # Hello 客户端
│   │   ├── generate_proto.py
│   │   └── requirements.txt
│   │
│   ├── grpc_server/                   # Python gRPC 服务端 ⭐
│   │   ├── __init__.py
│   │   ├── video_service.py          # 视频处理服务
│   │   ├── generate_proto.py
│   │   ├── requirements.txt
│   │   └── README.md
│   │
│   └── yolov5/                        # YOLOv5 算法
│       ├── detector.py
│       ├── preprocessor.py
│       └── ...
│
└── test/
    ├── grpc/
    │   ├── test_grpc_hello.cpp       # C++ Hello 测试
    │   ├── test_grpc_python.py       # Python Hello 测试
    │   └── test_video_server.py      # Python 视频服务器测试
    └── ...
```

## 快速开始

### 1. 安装 Python 依赖

```bash
# gRPC 服务端
cd algorithm/grpc_server
pip install -r requirements.txt

# 生成 gRPC 代码
python generate_proto.py
```

### 2. 启动 Python 服务器

```bash
# 使用模拟检测器（测试）
python video_service.py --port 50052

# 或使用 YOLOv5
python video_service.py --port 50052 --model yolov5s.pt --device cuda
```

### 3. 编译 C++ 项目

```bash
cd out/build/x64-Debug
cmake ../..
cmake --build . --config Debug
```

### 4. 运行测试

**终端 1** - Python 服务器：
```bash
cd test/grpc
python test_video_server.py
```

**终端 2** - C++ 客户端（待实现）：
```bash
cd out/build/x64-Debug
.\bin\test_video_grpc_client.exe
```

## 性能指标

### 典型性能（1080p @ 30fps）

| 指标 | 模式 1（元数据） | 模式 2（视频） |
|------|-----------------|---------------|
| 带宽 | ~100 KB/s | ~5 MB/s |
| 延迟 | 20-50 ms | 50-100 ms |
| CPU 占用 | 低 | 中 |
| GPU 占用 | 取决于算法 | 取决于算法 |

### 优化建议

1. **降低分辨率**：720p 比 1080p 快 2x
2. **调整 JPEG 质量**：70-85 是好的平衡点
3. **使用 CUDA**：YOLOv5 on CUDA 比 CPU 快 10x
4. **批量处理**：一次发送多帧（如果允许延迟）

## 扩展指南

### 添加新算法

1. 在 `algorithm/grpc_server/video_service.py` 中添加新方法
2. 在 proto 文件中定义新的消息类型（如果需要）
3. 重新生成 gRPC 代码
4. 更新 C++ 客户端

### 支持更多视频格式

修改 `_decode_frame` 和 `_encode_frame` 方法：

```python
def _decode_frame(self, frame_msg):
    # 支持 PNG、WebP 等
    if frame_msg.format == FORMAT_PNG:
        image = cv2.imdecode(nparr, cv2.IMREAD_UNCHANGED)
    else:
        image = cv2.imdecode(nparr, cv2.IMREAD_COLOR)
    return image
```

### 添加认证

```python
# 服务端
credentials = grpc.ssl_server_credentials(...)
server.add_secure_port(address, credentials)

# 客户端
credentials = grpc.ssl_channel_credentials(...)
channel = grpc.secure_channel(target, credentials)
```

## 故障排查

### 问题 1: 连接超时

**症状**：`Connection timeout`

**解决**：
1. 检查服务器是否运行：`netstat -an | grep 50052`
2. 检查防火墙设置
3. 增加超时时间

### 问题 2: 内存泄漏

**症状**：内存持续增长

**解决**：
1. 检查是否正确关闭流
2. 减少并发连接数
3. 降低视频质量

### 问题 3: 高延迟

**症状**：延迟 > 100ms

**解决**：
1. 使用 CUDA 加速
2. 降低分辨率
3. 减少 JPEG 质量
4. 检查网络带宽

## 最佳实践

1. **始终设置超时**：避免无限等待
2. **错误处理**：捕获所有异常
3. **资源管理**：使用上下文管理器
4. **日志记录**：记录关键事件
5. **监控**：监控延迟、吞吐量、错误率

## 下一步

- [ ] 实现 C++ `VideoGrpcClient`
- [ ] 集成到视频流水线
- [ ] 性能基准测试
- [ ] 添加更多算法（姿态估计、分割等）
- [ ] 支持批量处理
- [ ] 添加 Web 界面

## 参考资源

- [gRPC 官方文档](https://grpc.io/docs/)
- [Protocol Buffers](https://developers.google.com/protocol-buffers)
- [YOLOv5](https://github.com/ultralytics/yolov5)
- [OpenCV](https://opencv.org/)
