# VideoPipeline OpenVINO 测试

## 概述

本测试验证 VideoPipeline 中 OpenVINO 后端的集成，测试完整的 puller-decoder-openvino 流程。

## 测试目标

1. ✅ 验证 OpenVINOBackend 在 VideoPipeline 中的正确集成
2. ✅ 测试视频帧从拉流、解码到 OpenVINO 推理的完整流程
3. ✅ 验证零拷贝架构的有效性（YUV -> OpenVINO）
4. ✅ 测试优雅关闭和资源释放

## 编译

```bash
cd build
cmake ..
make test_video_pipeline_openvino
```

或在 Windows 上使用 Visual Studio 构建 `test_video_pipeline_openvino` 项目。

## 运行测试

### 基本用法

```bash
# 使用默认配置（无模型，将使用 NullBackend）
./bin/test_video_pipeline_openvino

# 指定流地址
./bin/test_video_pipeline_openvino "http://127.0.0.1/live/proxy_cam1.live.flv"

# 指定流地址和模型路径
./bin/test_video_pipeline_openvino \
    "http://127.0.0.1/live/proxy_cam1.live.flv" \
    "/path/to/model.xml"

# 完整参数
./bin/test_video_pipeline_openvino \
    "http://127.0.0.1/live/proxy_cam1.live.flv" \
    "/path/to/model.xml" \
    "CPU" \
    60
```

### 参数说明

| 参数 | 说明 | 默认值 |
|------|------|--------|
| stream_url | 视频流地址 | `http://127.0.0.1/live/proxy_cam1.live.flv` |
| model_path | OpenVINO 模型路径 (.xml) | 空（使用 NullBackend） |
| device | 推理设备 | `CPU` |
| duration | 测试持续时间（秒） | `60` |

### 设备选项

- `CPU`: 使用 CPU 进行推理（推荐用于测试）
- `GPU`: 使用集成显卡或独立显卡
- `MYRIAD`: 使用 Intel Movidius NCS
- `HDDL`: 使用 Intel HDDL-R

## 测试流程

```
┌──────────┐     ┌──────────┐     ┌──────────────┐
│  Puller  │────▶│ Decoder  │────▶│ OpenVINO     │
│ (FLV/RTSP)│    │ (FFmpeg) │    │ Backend      │
└──────────┘     └──────────┘     └──────────────┘
                                        │
                                        ▼
                                 ┌──────────────┐
                                 │ Inference    │
                                 │ Engine       │
                                 └──────────────┘
```

### 数据流

1. **Puller**: 从 HTTP-FLV/RTSP 流拉取视频数据
2. **Decoder**: 使用 FFmpeg 解码为 YUV420P 格式
3. **OpenVINO Backend**: 
   - 接收 YUV 原始数据（零拷贝）
   - 创建 TensorData 张量
   - 执行 OpenVINO 推理
   - 返回检测结果

## 预期输出

```
======================================================================
# VideoPipeline OpenVINO Integration Test
# Testing Puller -> Decoder -> OpenVINO Backend
======================================================================

Test Configuration:
  Stream URL: http://127.0.0.1/live/proxy_cam1.live.flv
  Model Path: /path/to/model.xml
  Device: CPU
  Channel ID: 1
  Test Duration: 60s

Pipeline Config:
  Algorithm: OpenVINO
  Decoder threads: 2
  Queue sizes: raw=64, decoded=16

Creating VideoPipeline with OpenVINO backend...
[OpenVINOBackend] Initialized: model=/path/to/model.xml, device=CPU, batch=1

Starting VideoPipeline...
✓ VideoPipeline started successfully

Waiting for frames...
(Press Ctrl+C to stop early)

--- Statistics at 5s ---
  Received:  150 frames
  Decoded:   148 frames
  Processed: 145 frames
  FPS (recv): 30
  FPS (proc): 29

[Shutdown] Stopping VideoPipeline...
[Shutdown] VideoPipeline stopped
[Shutdown] IO context stopped
[Shutdown] All resources released

======================================================================
# Final Statistics
======================================================================
Test duration: 60s

Frame Statistics:
  Received:  1800 frames
  Decoded:   1795 frames
  Processed: 1790 frames

Performance:
  Avg recv FPS: 30
  Avg proc FPS: 29

----------------------------------------------------------------------
# Test Result
----------------------------------------------------------------------
✓ PASSED: Frames received: 1800
✓ PASSED: Frames decoded: 1795
✓ PASSED: Frames processed by OpenVINO: 1790
----------------------------------------------------------------------
# Overall: TEST PASSED ✓
----------------------------------------------------------------------
```

## 故障排查

### 问题 1: 无法连接到流

**症状**: `Failed to start VideoPipeline`

**解决方案**:
1. 检查流地址是否正确
2. 确认 ZLMediaKit 服务器正在运行
3. 测试流是否可访问：`ffprobe <stream_url>`

### 问题 2: OpenVINO 模型加载失败

**症状**: `[OpenVINOBackend] Failed to load model`

**解决方案**:
1. 确认模型路径正确且文件存在
2. 检查模型格式是否为 OpenVINO IR (.xml + .bin)
3. 验证 OpenVINO 运行时已正确安装

### 问题 3: 没有帧被处理

**症状**: `Processed: 0 frames`

**可能原因**:
1. 未提供模型路径（会使用 NullBackend）
2. OpenVINO 后端初始化失败
3. 解码器未正常工作

**解决方案**:
1. 检查日志输出中的错误信息
2. 确保提供了有效的模型路径
3. 验证解码器配置正确

### 问题 4: 性能低下

**症状**: FPS 远低于预期

**优化建议**:
1. 使用 GPU 设备而非 CPU
2. 调整队列大小以减少延迟
3. 启用异步推理模式
4. 减少批处理大小

## 高级配置

### 调整队列大小

```cpp
config.decoder.raw_queue_size = 128;      // 增加原始包队列
config.decoder.decoded_queue_size = 32;   // 增加解码帧队列
```

### 修改置信度阈值

```cpp
config.algorithm.openvino.confidence_threshold = 0.3f;  // 降低阈值以检测更多目标
```

### 启用批处理

```cpp
config.algorithm.openvino.batch_size = 4;  // 批处理大小
```

## 性能基准

在不同硬件上的预期性能（1920x1080 @ 30fps）：

| 设备 | FPS | 延迟 | 备注 |
|------|-----|------|------|
| Intel i7-10700K (CPU) | 25-30 | ~40ms | 单路流 |
| Intel Iris Xe (GPU) | 30+ | ~20ms | 单路流 |
| NVIDIA GTX 1660 (GPU) | 30+ | ~15ms | 需要 CUDA 支持 |
| Intel Movidius NCS | 15-20 | ~60ms | 低功耗设备 |

## 相关文档

- [VideoPipeline 架构文档](../../../docs/video_pipeline_architecture.md)
- [OpenVINO 后端实现](../include/videopipeline/backends/openvino_backend.h)
- [零拷贝架构说明](../../../algorithm/CPP_ZERO_COPY_ARCHITECTURE.md)

## 贡献

如需改进测试或报告问题，请提交 Issue 或 Pull Request。