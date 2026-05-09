# VideoPipeline OpenVINO 测试 - 快速开始指南

## 🚀 快速开始

### 1. 编译项目

```bash
cd build
cmake ..
make test_video_pipeline_openvino
```

或在 Windows Visual Studio 中构建 `test_video_pipeline_openvino` 项目。

### 2. 运行测试

#### 方式 A: 使用脚本（推荐）

**Windows:**
```cmd
cd modules\videopipeline\test
run_openvino_test.bat
```

**Linux/macOS:**
```bash
cd modules/videopipeline/test
chmod +x run_openvino_test.sh
./run_openvino_test.sh
```

#### 方式 B: 直接运行可执行文件

```bash
# 基本测试（无模型，使用 NullBackend）
./bin/test_video_pipeline_openvino

# 指定流地址
./bin/test_video_pipeline_openvino "http://127.0.0.1/live/proxy_cam1.live.flv"

# 完整参数
./bin/test_video_pipeline_openvino \
    "http://127.0.0.1/live/proxy_cam1.live.flv" \
    "/path/to/yolov5.xml" \
    "CPU" \
    60
```

## 📋 前置条件

### 必需
- ✅ C++20 编译器
- ✅ Boost.Asio
- ✅ FFmpeg
- ✅ OpenCV
- ✅ ZLMediaKit 服务器（用于拉流测试）

### 可选（用于真实 OpenVINO 推理）
- ✅ OpenVINO Runtime
- ✅ OpenVINO IR 模型文件 (.xml + .bin)

## 🎯 测试场景

### 场景 1: 基础连通性测试（无需模型）

测试 puller 和 decoder 是否正常工作：

```bash
./bin/test_video_pipeline_openvino
```

**预期结果**: 
- ✓ 成功连接到流
- ✓ 帧被正确解码
- ⚠ Processed = 0（因为没有模型）

### 场景 2: 完整 OpenVINO 推理测试

提供模型进行真实推理：

```bash
./bin/test_video_pipeline_openvino \
    "http://127.0.0.1/live/proxy_cam1.live.flv" \
    "/path/to/model.xml" \
    "CPU"
```

**预期结果**:
- ✓ 成功加载模型
- ✓ 帧被推理处理
- ✓ 检测结果回调被触发

### 场景 3: GPU 加速测试

使用 GPU 设备进行推理：

```bash
./bin/test_video_pipeline_openvino \
    "http://127.0.0.1/live/proxy_cam1.live.flv" \
    "/path/to/model.xml" \
    "GPU"
```

## 🔍 验证测试结果

### 成功的标志

```
✓ PASSED: Frames received: XXXX
✓ PASSED: Frames decoded: XXXX
✓ PASSED: Frames processed by OpenVINO: XXXX
----------------------------------------------------------------------
# Overall: TEST PASSED ✓
```

### 失败的常见原因

1. **无法连接流**
   ```
   ✗ FAILED: No frames received from stream
   ```
   → 检查 ZLMediaKit 是否运行，流地址是否正确

2. **解码失败**
   ```
   ✗ FAILED: No frames decoded
   ```
   → 检查 FFmpeg 配置，视频编码格式是否支持

3. **OpenVINO 初始化失败**
   ```
   [OpenVINOBackend] Failed to load model
   ```
   → 检查模型路径、文件格式、OpenVINO 安装

## 📊 性能调优

### 提高 FPS

1. **使用 GPU**:
   ```cpp
   config.algorithm.openvino.device = "GPU";
   ```

2. **调整队列大小**:
   ```cpp
   config.decoder.raw_queue_size = 128;
   config.decoder.decoded_queue_size = 32;
   ```

3. **减少分辨率**（在解码前缩放）

### 降低延迟

1. **减小队列**:
   ```cpp
   config.decoder.raw_queue_size = 32;
   config.decoder.decoded_queue_size = 8;
   ```

2. **启用异步模式**:
   ```cpp
   config.algorithm.openvino.batch_size = 1;  // 单帧推理
   ```

## 🐛 调试技巧

### 运行诊断脚本

在运行测试前，先检查环境配置：

```powershell
# Windows
cd modules\videopipeline\test
.\diagnostic_openvino.ps1
```

这会检查：
- ✓ OpenVINO DLL 文件是否存在
- ✓ 关键 DLL（frontend、plugin）是否完整
- ✓ 模型文件（.xml + .bin）是否在位
- ✓ 环境变量是否正确设置
- ✓ 可执行文件是否已编译

### 查看详细日志

测试程序已集成 LogManager，日志输出到控制台和文件。

### 检查特定组件

1. **仅测试 Puller**:
   查看 `test_zlm_puller.cpp`

2. **仅测试 Decoder**:
   查看 `test_ffmpeg_decoder.cpp`

3. **仅测试 OpenVINO**:
   查看 `modules/alg/test/test_inference.cpp`

### 使用 GDB/LLDB

```bash
gdb ./bin/test_video_pipeline_openvino
(gdb) run
(gdb) bt  # 如果崩溃，查看堆栈
```

## 📝 自定义测试

### 修改测试时长

编辑 `test_video_pipeline_openvino.cpp`:
```cpp
int test_duration_sec = 120;  // 改为 120 秒
```

### 添加自定义回调

```cpp
pipeline->setResultCallback([](int ch_id, const DetectionResult& result) {
    // 你的自定义逻辑
    std::cout << "Detected " << result.boxes.size() << " objects" << std::endl;
});
```

### 更改统计频率

```cpp
if (elapsed_sec % 1 == 0) {  // 每秒打印（原来是 5 秒）
    // 打印统计
}
```

## 🔄 CI/CD 集成

### GitHub Actions 示例

```yaml
name: OpenVINO Test
on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      
      - name: Build
        run: |
          mkdir build && cd build
          cmake ..
          make test_video_pipeline_openvino
      
      - name: Run Test
        run: |
          cd modules/videopipeline/test
          ./run_openvino_test.sh "test_stream_url" "" "CPU" 10
```

## 📚 相关资源

- [完整测试文档](README_OPENVINO_TEST.md)
- [VideoPipeline 架构](../../../docs/video_pipeline_architecture.md)
- [OpenVINO 官方文档](https://docs.openvino.ai/)
- [零拷贝架构](../../../algorithm/CPP_ZERO_COPY_ARCHITECTURE.md)

## ❓ 常见问题

**Q: 没有模型文件可以测试吗？**  
A: 可以！不提供模型路径时会使用 NullBackend，仍然可以测试 puller 和 decoder。

**Q: 如何获取测试用的 OpenVINO 模型？**  
A: 可以从 OpenVINO Model Zoo 下载预训练模型，或转换自己的 PyTorch/TensorFlow 模型。

**Q: 测试需要真实的视频流吗？**  
A: 建议使用真实流，但也可以使用本地视频文件通过 FFmpeg 推流到 ZLMediaKit。

**Q: 如何在 Docker 中运行测试？**  
A: 需要挂载视频设备和模型文件，确保容器内有 OpenVINO 运行时。

## 💡 提示

1. **首次运行**: 先不带模型测试，确保基础流程正常
2. **性能测试**: 至少运行 60 秒以获得稳定的 FPS 数据
3. **多路测试**: 可以启动多个实例测试并发性能
4. **监控资源**: 使用 `htop` 或任务管理器监控 CPU/GPU 使用率

---

**Happy Testing! 🎉**