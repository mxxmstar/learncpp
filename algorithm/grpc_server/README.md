# Python gRPC 视频处理服务端

Python gRPC 服务端，接收 C++ 发送的视频帧，运行算法处理后返回结果。

## 目录结构

```
algorithm/grpc_server/
├── __init__.py                  # 模块初始化
├── video_service.py             # 视频处理服务实现
├── generate_proto.py            # 生成 gRPC 代码脚本
├── requirements.txt             # Python 依赖
├── video_processing_pb2.py     # 生成的 protobuf 代码（自动生成）
└── video_processing_pb2_grpc.py # 生成的 gRPC 代码（自动生成）
```

## 安装依赖

```bash
cd algorithm/grpc_server
pip install -r requirements.txt
```

## 生成 gRPC 代码

```bash
python generate_proto.py
```

这会生成：
- `video_processing_pb2.py` - Protobuf 消息定义
- `video_processing_pb2_grpc.py` - gRPC 服务定义

## 启动服务器

### 1. 使用模拟检测器（测试用）

```bash
python video_service.py --port 50052
```

### 2. 使用 YOLOv5 模型

```bash
python video_service.py --port 50052 --model yolov5s.pt --device cuda
```

**参数说明：**
- `--port`: 监听端口（默认 50052）
- `--model`: YOLOv5 模型路径
- `--device`: 推理设备（cpu 或 cuda）

## API 说明

### VideoProcessingService

提供两个双向流式 RPC 方法：

#### 1. DetectObjects - 对象检测（返回元数据）

**场景**：C++ 发送视频帧，Python 返回检测结果（边界框、类别等）

```protobuf
rpc DetectObjects (stream VideoFrame) returns (stream DetectionResult);
```

**用途**：
- C++ 进行后处理（绘制、跟踪等）
- 低带宽消耗（只传输元数据）

#### 2. ProcessAndReturnVideo - 处理并返回视频

**场景**：C++ 发送视频帧，Python 返回处理后的视频（带标注）

```protobuf
rpc ProcessAndReturnVideo (stream VideoFrame) returns (stream ProcessedFrame);
```

**用途**：
- Python 完成所有处理
- C++ 直接显示或保存
- 高带宽消耗（传输完整视频）

## 测试

### 启动测试服务器

```bash
cd test/grpc
python test_video_server.py
```

这会：
1. 创建测试视频
2. 启动 gRPC 服务器（端口 50052）
3. 等待客户端连接

### 从 C++ 客户端连接

确保 C++ 客户端已编译，然后运行：

```bash
cd out/build/x64-Debug
.\bin\test_video_grpc_client.exe
```

## 集成 YOLOv5

要使用真实的 YOLOv5 检测，需要：

1. **安装 YOLOv5 依赖**：
   ```bash
   pip install torch torchvision
   cd algorithm/yolov5
   pip install -r requirements.txt
   ```

2. **下载模型**：
   ```bash
   # 从 Ultralytics 下载预训练模型
   wget https://github.com/ultralytics/yolov5/releases/download/v7.0/yolov5s.pt
   ```

3. **启动服务器**：
   ```bash
   python video_service.py --model yolov5s.pt --device cuda
   ```

## 性能优化建议

### 1. 视频编码质量

```python
# 在 _encode_frame 中调整 quality 参数
quality = 85  # 平衡质量和大小
# quality = 70  # 更低带宽
# quality = 95  # 更高质量
```

### 2. 帧率控制

在 C++ 客户端控制发送帧率：

```cpp
// 每秒 30 帧
std::this_thread::sleep_for(std::chrono::milliseconds(33));
```

### 3. 并发处理

服务器使用线程池处理多个连接：

```python
server = grpc.server(
    futures.ThreadPoolExecutor(max_workers=10),  # 10个工作线程
    ...
)
```

### 4. 消息大小限制

默认限制 50MB，可根据需要调整：

```python
options=[
    ('grpc.max_send_message_length', 100 * 1024 * 1024),   # 100MB
    ('grpc.max_receive_message_length', 100 * 1024 * 1024),
]
```

## 常见问题

### Q: 连接失败
A: 确保服务器正在运行，防火墙允许端口 50052

### Q: 内存占用高
A: 降低 JPEG 编码质量，或减少并发连接数

### Q: 延迟高
A: 
- 使用 CUDA 加速
- 降低视频分辨率
- 减少编码质量

### Q: YOLOv5 加载失败
A: 检查模型路径是否正确，依赖是否安装

## 架构优势

✅ **双向流式通信**：实时性好，延迟低  
✅ **灵活的后处理**：支持两种模式（元数据/视频）  
✅ **易于扩展**：可添加更多算法  
✅ **独立部署**：Python 和 C++ 解耦  
✅ **负载均衡**：支持多客户端连接  

## 下一步

1. 实现 C++ 客户端（`grpc/src/video_grpc_client.cpp`）
2. 集成到视频流水线
3. 性能测试和优化
4. 添加更多算法支持
