# GrpcAlgorithmProcessor 集成测试指南

## 测试目标

验证 `GrpcAlgorithmProcessor` 能够：
1. ✅ 将视频帧从 C++ 发送到 Python gRPC 服务器
2. ✅ Python 端显示视频流
3. ✅ Python 返回检测结果
4. ✅ C++ 在回调中接收并打印检测结果

## 目录结构

```
test/grpc/
├── test_grpc_algorithm_integration.cpp  # 集成测试程序
└── README_INTEGRATION_TEST.md           # 本文档
```

## 前置条件

### 1. Python 环境

确保已安装 Python 依赖：

```bash
cd algorithm/grpc_server
pip install -r requirements.txt
```

### 2. 生成 gRPC 代码

```bash
cd algorithm/grpc_server
python generate_proto.py
```

这会生成：
- `video_processing_pb2.py`
- `video_processing_pb2_grpc.py`

### 3. 准备测试视频

确保项目根目录有测试视频文件：

```bash
# 如果没有，可以复制一个 MP4 文件到项目根目录
cp /path/to/your/video.mp4 d:\file_mx\aaaaa\learncpp\test.mp4
```

## 测试步骤

### 步骤 1: 编译项目

```bash
cd out/build/x64-Debug
cmake ../..
cmake --build . --config Debug --target test_grpc_algorithm_integration
```

### 步骤 2: 启动 Python gRPC 服务器

**终端 1** - 启动 Python 服务器：

```bash
cd algorithm/grpc_server
python video_service.py --port 50052
```

**预期输出**：
```
[VideoService] Server started on [::]:50052
[VideoService] Model: Mock
[VideoService] Device: cpu
[VideoService] Ready to accept connections...
```

此时会弹出一个窗口 "Python Video Stream"，用于显示接收到的视频帧。

### 步骤 3: 运行 C++ 测试程序

**终端 2** - 运行 C++ 客户端：

```bash
cd out/build/x64-Debug
.\bin\test_grpc_algorithm_integration.exe
```

或者指定视频文件：

```bash
.\bin\test_grpc_algorithm_integration.exe path\to\video.mp4
```

**预期输出**：
```
######################################################################
# GrpcAlgorithmProcessor Integration Test
# C++ → Python gRPC Video Stream + Detection
######################################################################

Video file: test.mp4
Video info:
  Resolution: 1920x1080
  FPS: 30.0
  Total frames: 900

Processor config:
  Type: gRPC Python
  Server: localhost:50052
  Target FPS: 10

Starting algorithm processor...
✓ Algorithm processor started

Processing video frames...
(Press 'q' to quit)

[Callback] Frame: frame_1
  Algorithm: YOLOv5 (via gRPC)
  Processing time: 45ms
  Detected objects: 2
    [0] person conf=0.92 pos=(120,80) size=100x200
    [1] car conf=0.87 pos=(300,200) size=150x100

Progress: 10/900 frames (sent: 10)
...
```

## 测试验证点

### 1. Python 端验证

✅ **视频窗口显示**
- 应该看到 "Python Video Stream" 窗口
- 窗口中显示从 C++ 发送的视频帧
- 视频流畅播放

✅ **控制台输出**
```
[VideoService] DetectObjects stream started
[VideoService] Processed 30 frames, Avg time: 45ms/frame
```

### 2. C++ 端验证

✅ **连接成功**
```
✓ Algorithm processor started
```

✅ **检测结果回调**
```
[Callback] Frame: frame_1
  Algorithm: YOLOv5 (via gRPC)
  Processing time: 45ms
  Detected objects: 2
    [0] person conf=0.92 ...
```

✅ **统计信息**
```
--- Processing Summary ---
Total frames read: 900
Frames sent to Python: 300
Duration: 30s
Read FPS: 30
Send FPS: 10

--- Processor Statistics ---
Frames processed: 300
Frames failed: 0
Avg processing time: 45ms
Result FPS: 22.2
```

## 常见问题

### Q1: 连接失败

**症状**：
```
Error: Failed to start algorithm processor
Please make sure Python gRPC server is running
```

**解决**：
1. 确认 Python 服务器正在运行
2. 检查端口是否正确（默认 50052）
3. 检查防火墙设置

```bash
# 检查端口
netstat -an | grep 50052
```

### Q2: Python 窗口不显示视频

**症状**：窗口打开但黑屏

**解决**：
1. 检查 OpenCV 是否正确安装
2. 确认视频帧编码正确
3. 查看 Python 控制台是否有错误

### Q3: 没有收到检测结果

**症状**：C++ 端没有 `[Callback]` 输出

**解决**：
1. 检查 Python 端是否正常运行
2. 查看 Python 控制台错误信息
3. 确认 gRPC 连接状态

### Q4: 性能问题

**症状**：延迟高或帧率低

**优化建议**：
1. 降低发送帧率（修改 `grpc_target_fps`）
2. 降低 JPEG 质量（修改 `EncodeToJpeg` 的 quality 参数）
3. 使用 CUDA 加速（如果有 GPU）

## 测试场景

### 场景 1: 基本功能测试

**目标**：验证基本通信正常

**步骤**：
1. 启动 Python 服务器
2. 运行 C++ 测试
3. 观察视频显示和检测结果

**预期**：
- Python 显示视频
- C++ 打印检测结果
- 无错误

### 场景 2: 长时间运行测试

**目标**：验证稳定性

**步骤**：
1. 使用较长的视频文件（> 5 分钟）
2. 运行测试
3. 观察内存使用和错误

**预期**：
- 无内存泄漏
- 无崩溃
- 稳定的帧率

### 场景 3: 不同视频格式测试

**目标**：验证兼容性

**步骤**：
1. 测试不同分辨率的视频
2. 测试不同编码格式（H.264, H.265）
3. 测试不同帧率

**预期**：
- 所有视频都能正常处理
- 自动适应不同参数

## 性能基准

### 典型性能指标

| 指标 | 期望值 | 说明 |
|------|--------|------|
| 连接时间 | < 1s | 建立 gRPC 连接 |
| 单帧处理时间 | 20-50ms | Python 端处理 |
| 端到端延迟 | 50-100ms | 从发送到接收结果 |
| 发送帧率 | 10 fps | 可配置 |
| 结果帧率 | 8-10 fps | 略低于发送帧率 |

### 优化建议

1. **降低发送帧率**
   ```cpp
   config.grpc_target_fps = 5; // 每秒 5 帧
   ```

2. **降低 JPEG 质量**
   ```cpp
   auto jpeg_data = EncodeToJpeg(frame, 70); // 质量 70
   ```

3. **使用 CUDA**
   ```bash
   python video_service.py --model yolov5s.pt --device cuda
   ```

## 下一步

测试通过后，可以：

1. **集成到 VideoPipeline**
   - 在 VideoPipeline 中使用 `GrpcAlgorithmProcessor`
   - 从解码队列取帧发送
   - 接收结果并绘制 OSD

2. **切换到原生 C++ 处理器**
   - 实现 `NativeCppAlgorithmProcessor`
   - 只需修改配置即可切换

3. **添加更多算法**
   - 目标跟踪
   - 行为分析
   - 姿态估计

## 总结

这个测试验证了：
- ✅ C++ → Python gRPC 通信
- ✅ 视频流传输
- ✅ 异步检测结果返回
- ✅ 回调机制工作正常
- ✅ 统计信息收集

架构设计支持未来无缝切换到 C++ 算法处理器！🎉
