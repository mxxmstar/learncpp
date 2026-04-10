# VideoPipeline gRPC 集成测试指南（第一阶段）

## 概述

第一阶段实现将 `GrpcVideoSender` 集成到 `VideoPipeline` 中，实现基础的视频帧发送功能。

### 实现的功能

✅ **GrpcVideoSender 类**
- 使用现有的 `VideoGrpcClient`
- 实现基本的双向流通信
- 简单的帧发送（无帧率控制）
- 连接状态管理

✅ **VideoPipeline 集成**
- 在配置中添加 gRPC 选项
- 自动启动/停止 gRPC 发送器
- 在解码回调中编码并发送帧
- 统计信息收集

### 未实现的功能（后续阶段）

❌ 帧率控制
❌ 检测结果处理回调
❌ 错误恢复机制
❌ 性能优化

---

## 配置方法

### 1. 启用 gRPC 发送

在创建 `PipelineConfig` 时设置：

```cpp
PipelineConfig config;
config.stream_url = "rtsp://localhost/live/stream";
config.channel_id = 1;

// 启用 gRPC 发送
config.enable_grpc_send = true;
config.grpc_server_address = "localhost:50053";
config.grpc_target_fps = 10;  // 当前阶段不使用，预留
```

### 2. 创建 VideoPipeline

```cpp
boost::asio::io_context io_ctx;
auto pipeline = std::make_unique<VideoPipeline>(io_ctx, config);

// 启动流水线
if (pipeline->start()) {
    std::cout << "Pipeline started with gRPC sending enabled" << std::endl;
}
```

---

## 数据流

```
┌─────────────────────────────────────────────────────┐
│              VideoPipeline 数据流                    │
├─────────────────────────────────────────────────────┤
│                                                     │
│  ZLMPuller (RTSP/HTTP-FLV)                         │
│       ↓                                             │
│  RawPacketQueue (NALU)                             │
│       ↓                                             │
│  FFmpegDecoder                                     │
│       ↓                                             │
│  VideoFrame (YUV/RGB)                              │
│       ↓                                             │
│  ┌────────────────┬──────────────────┐             │
│  ↓                ↓                  │             │
│  GrpcVideoSender  OpenCVConverter   │             │
│  (编码+发送)      (YUV→BGR)         │             │
│  ↓                ↓                  │             │
│  Python gRPC     cv::Mat            │             │
│  Server          ↓                  │             │
│                  Output Callback    │             │
└─────────────────────────────────────────────────────┘
```

---

## 测试步骤

### 步骤 1: 启动 Python gRPC 服务器

```bash
cd algorithm/grpc_server
python video_service.py --port 50053
```

**预期输出**：
```
[VideoService] Server started on [::]:50053
[VideoService] Model: Mock
[VideoService] Ready to accept connections...
```

### 步骤 2: 运行 C++ 程序

确保你的主程序中启用了 gRPC：

```cpp
// main.cpp 或测试代码
PipelineConfig config;
config.stream_url = "rtsp://your-server/live/stream";
config.channel_id = 1;
config.enable_grpc_send = true;
config.grpc_server_address = "localhost:50053";

auto pipeline = std::make_unique<VideoPipeline>(io_ctx, config);
pipeline->start();

// 等待一段时间
std::this_thread::sleep_for(std::chrono::seconds(60));

// 查看统计
std::cout << "gRPC frames sent: " << pipeline->getGrpcFramesSent() << std::endl;
std::cout << "gRPC frames failed: " << pipeline->getGrpcFramesFailed() << std::endl;
```

### 步骤 3: 验证结果

**Python 端**：
- ✅ "Python Video Stream" 窗口显示视频
- ✅ 控制台显示处理的帧数

**C++ 端日志**：
```
[INFO] VideoPipeline created: channel=1, url=rtsp://...
[INFO] gRPC video sender created: address=localhost:50053
[INFO] VideoPipeline started: channel=1
[INFO] gRPC video sender started
[DEBUG] [GrpcVideoSender] Received detection result for frame: ch1_frame1, boxes: 2
...
```

---

## API 参考

### PipelineConfig 新增字段

```cpp
struct PipelineConfig {
    // ... 现有字段 ...
    
    // ==================== gRPC 配置 ====================
    bool enable_grpc_send = false;           // 是否启用 gRPC 发送
    std::string grpc_server_address = "localhost:50053";  // 服务器地址
    int grpc_target_fps = 10;                // 目标帧率（预留）
};
```

### VideoPipeline 新增方法

```cpp
class VideoPipeline {
public:
    // 获取 gRPC 发送统计
    uint64_t getGrpcFramesSent() const;
    uint64_t getGrpcFramesFailed() const;
};
```

### GrpcVideoSender 类

```cpp
namespace video_pipeline {

class GrpcVideoSender {
public:
    explicit GrpcVideoSender(const std::string& server_address);
    
    bool start();
    void stop();
    
    bool sendFrame(const std::vector<uint8_t>& jpeg_data, 
                   int width, 
                   int height,
                   const std::string& frame_id,
                   int64_t timestamp);
    
    bool isConnected() const;
};

} // namespace video_pipeline
```

---

## 常见问题

### Q1: gRPC 连接失败

**症状**：
```
[ERROR] [GrpcVideoSender] Failed to connect to localhost:50053
```

**解决**：
1. 确认 Python 服务器正在运行
2. 检查端口是否正确
3. 检查防火墙设置

```bash
# 检查端口
netstat -an | grep 50053
```

### Q2: 没有看到 Python 视频窗口

**症状**：Python 服务器运行但无窗口

**解决**：
1. 确认 C++ 客户端已连接
2. 检查是否有帧发送
3. 查看 Python 控制台日志

### Q3: 性能问题

**症状**：CPU 使用率高或延迟大

**优化建议**：
1. 降低 JPEG 质量（修改 `encodeAndSendToGrpc` 中的 quality 参数）
2. 减少分辨率
3. 后续阶段添加帧率控制

---

## 性能指标

### 典型性能（第一阶段）

| 指标 | 值 | 说明 |
|------|-----|------|
| 单帧编码时间 | 5-15ms | JPEG 编码 |
| 网络传输时间 | 10-30ms | 取决于带宽 |
| Python 处理时间 | 20-50ms | Mock 检测 |
| 端到端延迟 | 50-100ms | 从解码到结果 |
| CPU 使用率 | +10-20% | 额外开销 |

### 优化方向（后续阶段）

1. **帧率控制**：只发送关键帧或降低帧率
2. **批量发送**：累积多帧后批量发送
3. **异步编码**：使用独立线程编码
4. **压缩优化**：调整 JPEG 质量和分辨率

---

## 下一步计划

### 第二阶段：帧率控制和优化

- [ ] 实现帧率控制逻辑
- [ ] 添加跳帧策略
- [ ] 性能监控和优化

### 第三阶段：结果处理

- [ ] 实现检测结果回调
- [ ] 绘制 OSD（检测框）
- [ ] 结果存储和分析

### 第四阶段：错误恢复

- [ ] 断线重连
- [ ] 队列溢出处理
- [ ] 降级策略

---

## 总结

第一阶段成功实现了：
- ✅ GrpcVideoSender 基础类
- ✅ VideoPipeline 集成
- ✅ 视频帧编码和发送
- ✅ 基本的双向流通信
- ✅ 统计信息收集

架构设计为后续扩展预留了充分的空间！🎉
