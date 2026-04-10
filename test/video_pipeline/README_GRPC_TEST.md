# VideoPipeline gRPC 集成测试

## 测试文件

- `test/video_pipeline/test_video_pipeline_grpc.cpp` - 完整的集成测试

## 测试目标

验证 `GrpcVideoSender` 在 `VideoPipeline` 中的集成：

1. ✅ VideoPipeline 能正确创建和启动 GrpcVideoSender
2. ✅ 视频帧从解码到 gRPC 发送的完整流程
3. ✅ Python 端能接收并显示视频流
4. ✅ 统计信息正确收集
5. ✅ 错误处理和资源清理

## 编译测试

### 方法 1: 使用 CMake 选项

```bash
cd out/build/x64-Debug
cmake ../.. -DBUILD_VIDEO_PIPELINE_TESTS=ON
cmake --build . --target test_video_pipeline_test_video_pipeline_grpc
```

### 方法 2: 构建所有 video_pipeline 测试

```bash
cd out/build/x64-Debug
cmake --build . --config Debug
```

这会自动构建所有 video_pipeline 目录下的测试。

## 运行测试

### 前置条件

1. **启动 Python gRPC 服务器**

```bash
cd algorithm/grpc_server
python video_service.py --port 50053
```

预期输出：
```
[VideoService] Server started on [::]:50053
[VideoService] Ready to accept connections...
```

2. **准备视频流**

确保有一个可用的 RTSP/HTTP-FLV 流，或者使用测试视频文件。

### 运行测试程序

#### 基本用法（使用默认参数）

```bash
cd out/build/x64-Debug
.\bin\test_video_pipeline_test_video_pipeline_grpc.exe
```

#### 指定流地址

```bash
.\bin\test_video_pipeline_test_video_pipeline_grpc.exe rtsp://your-server/live/stream
```

#### 指定流地址和 gRPC 服务器

```bash
.\bin\test_video_pipeline_test_video_pipeline_grpc.exe rtsp://localhost/live/test localhost:50053
```

#### 指定测试时长

```bash
.\bin\test_video_pipeline_test_video_pipeline_grpc.exe rtsp://localhost/live/test localhost:50053 30
```

参数说明：
- 参数 1: 流 URL（默认: rtsp://localhost/live/test）
- 参数 2: gRPC 服务器地址（默认: localhost:50053）
- 参数 3: 测试时长秒数（默认: 60）

## 测试输出

### 启动阶段

```
######################################################################
# VideoPipeline gRPC Integration Test
# Testing GrpcVideoSender in VideoPipeline
######################################################################

Test Configuration:
  Stream URL: rtsp://localhost/live/test
  gRPC Server: localhost:50053
  Channel ID: 1
  Test Duration: 60s

Pipeline Config:
  Enable gRPC: YES
  Decoder threads: 2
  Queue sizes: raw=64, decoded=16

Creating VideoPipeline...

Starting VideoPipeline...
✓ VideoPipeline started successfully

Waiting for frames...
(Press Ctrl+C to stop early)
```

### 运行阶段（每 5 秒输出统计）

```
--- Statistics at 5s ---
  Received:  150 frames
  Decoded:   148 frames
  Processed: 148 frames
  gRPC Sent: 50 frames
  gRPC Fail: 0 frames
  FPS (recv): 30
  FPS (grpc): 10
  Fail rate: 0%

--- Statistics at 10s ---
  Received:  300 frames
  Decoded:   298 frames
  Processed: 298 frames
  gRPC Sent: 100 frames
  gRPC Fail: 0 frames
  FPS (recv): 30
  FPS (grpc): 10
  Fail rate: 0%
```

### 结束阶段

```
Stopping VideoPipeline...

======================================================================
# Final Statistics
======================================================================
Test duration: 60s

Frame Statistics:
  Received:  1800 frames
  Decoded:   1795 frames
  Processed: 1795 frames

gRPC Statistics:
  Sent:      600 frames
  Failed:    0 frames
  Success:   100%

Performance:
  Avg recv FPS: 30
  Avg grpc FPS: 10

----------------------------------------------------------------------
# Test Result
----------------------------------------------------------------------
✓ PASSED: Frames sent via gRPC: 600
✓ PASSED: Acceptable failure rate
✓ PASSED: Frames decoded: 1795
----------------------------------------------------------------------
# Overall: TEST PASSED ✓
----------------------------------------------------------------------
```

## 验证点

### Python 端验证

✅ **视频窗口显示**
- 应该看到 "Python Video Stream" 窗口
- 窗口中显示实时视频
- 视频流畅无卡顿

✅ **控制台输出**
```
[VideoService] DetectObjects stream started
[VideoService] Processed 30 frames, Avg time: 45ms/frame
```

### C++ 端验证

✅ **连接成功**
- 日志显示 "gRPC video sender started"
- 无连接错误

✅ **帧发送正常**
- `getGrpcFramesSent()` > 0
- `getGrpcFramesFailed()` 接近 0
- 成功率 > 95%

✅ **统计信息准确**
- Received >= Decoded >= Processed
- gRPC Sent <= Decoded
- FPS 符合预期

✅ **资源清理**
- 停止后无内存泄漏
- 线程正常退出
- gRPC 连接正确关闭

## 常见问题

### Q1: 连接失败

**症状**：
```
[ERROR] [GrpcVideoSender] Failed to connect to localhost:50053
Error: Failed to start VideoPipeline
```

**解决**：
1. 确认 Python 服务器正在运行
2. 检查端口是否正确
3. 检查防火墙设置

```bash
# 检查端口占用
netstat -an | grep 50053
```

### Q2: 没有帧发送

**症状**：
```
gRPC Sent: 0 frames
✗ FAILED: No frames sent via gRPC
```

**解决**：
1. 检查流 URL 是否正确
2. 确认视频流可访问
3. 查看日志是否有解码错误

```bash
# 测试流是否可用
ffplay rtsp://your-server/live/stream
```

### Q3: 高失败率

**症状**：
```
gRPC Fail: 100 frames
Fail rate: 50%
```

**解决**：
1. 检查网络带宽
2. 降低 JPEG 质量（修改代码中的 quality 参数）
3. 降低分辨率
4. 检查 Python 服务器负载

### Q4: 性能问题

**症状**：
- CPU 使用率高
- 延迟大
- 帧率低

**优化建议**：
1. 减少解码线程数
2. 降低队列大小
3. 调整 JPEG 质量
4. 后续添加帧率控制

## 测试场景

### 场景 1: 基本功能测试

**目标**：验证基本通信正常

**步骤**：
1. 启动 Python 服务器
2. 运行测试程序（60 秒）
3. 观察统计信息

**预期**：
- ✓ 所有验证点通过
- ✓ 成功率 > 95%
- ✓ 无崩溃或错误

### 场景 2: 长时间稳定性测试

**目标**：验证长时间运行的稳定性

**步骤**：
1. 运行测试程序（300 秒 = 5 分钟）
2. 监控内存使用
3. 观察错误率

**命令**：
```bash
.\bin\test_video_pipeline_test_video_pipeline_grpc.exe rtsp://localhost/live/test localhost:50053 300
```

**预期**：
- ✓ 无内存泄漏
- ✓ 错误率保持稳定
- ✓ 无崩溃

### 场景 3: 多通道测试

**目标**：验证多路并发

**步骤**：
1. 启动多个测试实例
2. 每个实例使用不同的 channel_id
3. 观察系统负载

**命令**：
```bash
# 终端 1
.\bin\test_video_pipeline_test_video_pipeline_grpc.exe rtsp://server/stream1 localhost:50053 60

# 终端 2
.\bin\test_video_pipeline_test_video_pipeline_grpc.exe rtsp://server/stream2 localhost:50053 60
```

**预期**：
- ✓ 所有通道正常工作
- ✓ Python 服务器能处理多路
- ✓ 系统资源在合理范围

### 场景 4: 断线重连测试

**目标**：验证错误恢复

**步骤**：
1. 启动测试
2. 手动停止 Python 服务器
3. 重新启动 Python 服务器
4. 观察是否自动重连

**预期**：
- ✓ 检测到连接断开
- ✓ 自动尝试重连
- ✓ 重连后恢复正常

## 性能基准

### 典型性能指标

| 指标 | 期望值 | 说明 |
|------|--------|------|
| 连接时间 | < 2s | 建立 gRPC 连接 |
| 单帧编码时间 | 5-15ms | JPEG 编码 |
| 网络传输时间 | 10-30ms | 局域网 |
| Python 处理时间 | 20-50ms | Mock 检测 |
| 端到端延迟 | 50-100ms | 从解码到结果 |
| gRPC 发送 FPS | 8-10 | 取决于配置 |
| 成功率 | > 95% | 正常网络条件 |
| CPU 额外开销 | +10-20% | gRPC 发送 |

### 不同分辨率的性能

| 分辨率 | 编码时间 | 带宽 | 推荐 FPS |
|--------|---------|------|----------|
| 640x480 | 3-8ms | 50-100 KB/s | 10-15 |
| 1280x720 | 8-15ms | 150-300 KB/s | 5-10 |
| 1920x1080 | 15-30ms | 400-800 KB/s | 3-5 |

## 调试技巧

### 启用详细日志

在代码中设置日志级别：

```cpp
// 在 main.cpp 或配置中
spdlog::set_level(spdlog::level::debug);
```

### 查看网络流量

使用 Wireshark 或 tcpdump：

```bash
# Windows (需要管理员权限)
netsh trace start capture=yes report=no persistent=no

# 运行测试...

netsh trace stop
```

### 监控资源使用

**Windows**:
```powershell
# 监控进程
Get-Process test_video_pipeline_* | Select-Object CPU,WorkingSet
```

**Linux**:
```bash
# 监控资源
top -p $(pgrep test_video_pipeline)
```

## 下一步

测试通过后，可以：

1. **集成到生产环境**
   - 在实际应用中使用相同的配置
   - 监控长期运行稳定性

2. **第二阶段优化**
   - 添加帧率控制
   - 实现检测结果回调
   - 绘制 OSD

3. **扩展功能**
   - 支持更多编解码格式
   - 添加批量发送
   - 实现优先级队列

## 总结

这个测试验证了：
- ✅ GrpcVideoSender 正确集成到 VideoPipeline
- ✅ 视频帧编码和发送流程正常
- ✅ Python 端能接收和显示视频
- ✅ 统计信息准确可靠
- ✅ 资源管理正确

为第二阶段的优化奠定了坚实的基础！🎉
