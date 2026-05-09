# VideoPipeline 重构方案

## 🎯 重构目标

重构 `VideoPipeline` 以支持三种不同的数据处理路径，实现灵活的算法后端选择：

1. **路径 A**: YUV → OpenVINO（零拷贝，本地推理）
2. **路径 B**: YUV → BGR → OpenCV（预处理后本地处理）
3. **路径 C**: YUV → JPEG → gRPC → Python（远程算法）

---

## 📊 当前架构分析

### 现有问题

```
当前流程：
Puller → Decoder(YUV) → [YuvToBgrConverter] → BGR → [cv::imencode] → JPEG → gRPC → Python

问题：
1. ❌ 只支持 gRPC 远程算法
2. ❌ 不支持 OpenVINO 本地推理
3. ❌ 不支持 OpenCV 本地处理
4. ❌ 强制经过 BGR 转换（即使不需要）
5. ❌ 架构耦合，难以扩展
```

---

## 🏗️ 新架构设计

### 核心思想：**策略模式 + 插件化**

```
                    ┌─────────────┐
                    │ VideoPipeline│
                    └──────┬──────┘
                           │
                    ┌──────▼──────┐
                    │   Decoder    │
                    │  (YUV输出)   │
                    └──────┬──────┘
                           │
              ┌────────────┼────────────┐
              │            │            │
      ┌───────▼──────┐ ┌──▼────────┐ ┌─▼──────────┐
      │  Path A      │ │ Path B    │ │ Path C     │
      │  OpenVINO    │ │ OpenCV    │ │ gRPC       │
      │  (零拷贝)    │ │ (BGR)     │ │ (JPEG)     │
      └───────┬──────┘ └──┬────────┘ └─┬──────────┘
              │            │            │
      ┌───────▼──────┐ ┌──▼────────┐ ┌─▼──────────┐
      │OpenVINOEngine│ │OpenCVAlg  │ │GrpcSender  │
      └──────────────┘ └───────────┘ └────────────┘
```

---

## 🔧 详细设计方案

### 1. 数据流路径定义

#### 路径 A: YUV → OpenVINO（零拷贝）

```
Puller → Decoder(YUV) → TensorData(零拷贝) → OpenVINO Engine → Result
         ↓                                    ↓
      无预处理                        PrePostProcessor 自动处理：
      无格式转换                      - YUV/NV12/NV21 → RGB/BGR
      无内存拷贝                      - Resize
                                      - Normalize
                                      - UINT8 → FLOAT32
```

**特点**：
- ✅ 最低延迟（< 5ms）
- ✅ 真正的零拷贝（直接使用 VideoFrame 指针）
- ✅ CPU 开销最小（OpenVINO 内部优化）
- ✅ 支持多种输入格式（YUV420P/NV12/NV21）
- ✅ OpenVINO PrePostProcessor 自动处理所有预处理

**适用场景**：
- 高性能实时推理
- YOLOv5/YOLOv8 等目标检测
- 本地部署

---

#### 路径 B: YUV → BGR → OpenCV

```
Puller → Decoder(YUV) → [Preprocess] → BGR → OpenCV Algorithm → Result
                          ↓
                     - Resize
                     - Filter
                     - Color Convert
```

**特点**：
- ✅ 灵活预处理
- ✅ 支持多种 OpenCV 算法
- ✅ 易于调试和可视化
- ⚠️ 中等延迟（~10ms）
- ⚠️ 需要颜色空间转换

**适用场景**：
- 人脸检测/识别
- 运动检测
- 图像增强
- 需要可视化的场景

---

#### 路径 C: YUV → JPEG → gRPC → Python

```
Puller → Decoder(YUV) → [Preprocess] → JPEG → gRPC → Python Algorithm → Result
                          ↓
                     - Resize (可选)
                     - YUV→JPEG
```

**特点**：
- ✅ 支持复杂 Python 算法
- ✅ 算法可远程更新
- ✅ 负载均衡
- ⚠️ 较高延迟（~50ms）
- ⚠️ 网络依赖

**适用场景**：
- 复杂 AI 模型（LLM、多模态）
- 算法频繁更新
- 分布式部署
- Python 生态依赖

---

### 2. 模块接口设计

#### 2.1 算法后端接口

```cpp
/// @brief 算法后端接口
class IAlgorithmBackend {
public:
    virtual ~IAlgorithmBackend() = default;
    
    /// @brief 初始化算法
    virtual bool initialize(const AlgorithmConfig& config) = 0;
    
    /// @brief 处理帧（YUV 原始数据）
    virtual void processFrame(const VideoFrame& frame) = 0;
    
    /// @brief 处理帧（BGR Mat）
    virtual void processFrame(cv::Mat&& frame, int64_t pts) = 0;
    
    /// @brief 设置结果回调
    using ResultCallback = std::function<void(int channel_id, const DetectionResult& result)>;
    void setResultCallback(ResultCallback cb) { result_callback_ = std::move(cb); }
    
protected:
    ResultCallback result_callback_;
};
```

---

#### 2.2 OpenVINO 后端实现

```cpp
class OpenVINOB backend : public IAlgorithmBackend {
public:
    bool initialize(const AlgorithmConfig& config) override {
        // 加载 OpenVINO 模型
        engine_ = InferenceEngineFactory::Create("openvino_cpu", config.openvino);
        return engine_ != nullptr;
    }
    
    void processFrame(const VideoFrame& frame) override {
        // ✅ 零拷贝：直接从 YUV 创建 TensorData
        auto tensor = TensorData::FromRawData(
            frame.data[0],
            frame.width * frame.height,
            {1, 3, frame.height, frame.width},
            TensorDataType::UINT8
        );
        
        // 推理
        auto output = engine_->Infer(tensor);
        
        // 回调结果
        if (result_callback_) {
            result_callback_(channel_id_, convertToDetectionResult(output));
        }
    }
    
    void processFrame(cv::Mat&& frame, int64_t pts) override {
        // OpenVINO 优先使用 YUV，此方法作为备选
        LOG_MAIN_WARN_AT("OpenVINO backend prefers YUV input");
    }
    
private:
    std::unique_ptr<IInferenceEngine> engine_;
    int channel_id_;
};
```

---

### 📌 OpenVINO PrePostProcessor 说明

**重要**：OpenVINOBackend **不需要**手动进行颜色空间转换和预处理！

#### 工作原理

1. **用户侧**（OpenVINOBackend）：
   ```cpp
   // ✅ 直接使用 YUV/NV12/NV21 原始数据
   auto tensor = TensorData::FromRawData(
       frame.data[0],  // YUV 连续内存
       total_size,
       {1, height, width},  // [N, H, W]
       TensorDataType::UINT8
   );
   
   // 执行推理
   auto output = engine_->Infer(tensor);
   ```

2. **OpenVINO 内部**（PrePostProcessor）：
   ```
   Input (YUV420P/NV12/NV21)
       ↓
   Color Space Conversion (YUV → RGB/BGR)
       ↓
   Resize (到模型尺寸)
       ↓
   Normalize (除以 255)
       ↓
   Type Convert (UINT8 → FLOAT32)
       ↓
   Model Inference
       ↓
   Output
   ```

#### 优势

- ✅ **真正的零拷贝**：直接使用 VideoFrame 指针，无需 `cvtColor`
- ✅ **性能最优**：OpenVINO 内部使用 SIMD/AVX 优化
- ✅ **代码简洁**：无需手动处理预处理逻辑
- ✅ **支持多种格式**：YUV420P、NV12、NV21 均可直接输入

#### 配置要求

在加载模型时，需要配置 PrePostProcessor：

```cpp
// 在 OpenVinoCpuEngine::LoadModel() 中
auto preproc = ov::preprocess::PrePostProcessor(model_ptr);

// 设置输入格式
preproc.input().tensor().set_element_type(ov::element::u8);
preproc.input().tensor().set_layout("NHWC");  // 或 "NCHW"

// 设置颜色空间转换
if (input_format == "YUV420P") {
    preproc.input().preprocess().convert_color(ov::preprocess::ColorFormat::I420);
} else if (input_format == "NV12") {
    preproc.input().preprocess().convert_color(ov::preprocess::ColorFormat::NV12);
}

// 设置缩放和归一化
preproc.input().preprocess().resize(ov::preprocess::ResizeAlgorithm::RESIZE_LINEAR);
preproc.input().preprocess().convert_element_type(ov::element::f32);
preproc.input().preprocess().scale(255.0f);

// 应用预处理
model_ptr = preproc.build();
```

⚠️ **注意**：当前 `OpenVinoCpuEngine` 尚未集成 PrePostProcessor 配置，需要在后续版本中添加。

---

#### 2.3 OpenCV 后端实现

```cpp
class OpenCVBackend : public IAlgorithmBackend {
public:
    bool initialize(const AlgorithmConfig& config) override {
        // 初始化 OpenCV 算法
        if (config.opencv.algorithm_type == "face_detect") {
            cascade_.load(config.opencv.config_path);
        } else if (config.opencv.algorithm_type == "motion_detect") {
            motion_detector_ = std::make_unique<MotionDetector>();
        }
        return true;
    }
    
    void processFrame(const VideoFrame& frame) override {
        // OpenCV 需要 BGR，先转换
        cv::Mat bgr = yuv_to_bgr_converter_.Convert(
            frame.data[0], frame.data[1], frame.data[2],
            frame.width, frame.height
        );
        
        processFrame(std::move(bgr), frame.pts);
    }
    
    void processFrame(cv::Mat&& frame, int64_t pts) override {
        // 执行 OpenCV 算法
        DetectionResult result;
        
        if (cascade_.empty() == false) {
            // 人脸检测
            std::vector<cv::Rect> faces;
            cascade_.detectMultiScale(frame, faces);
            result.faces = faces;
        }
        
        // 回调结果
        if (result_callback_) {
            result_callback_(channel_id_, result);
        }
    }
    
private:
    cv::CascadeClassifier cascade_;
    std::unique_ptr<MotionDetector> motion_detector_;
    YuvToBgrConverter yuv_to_bgr_converter_;
    int channel_id_;
};
```

---

#### 2.4 gRPC 后端实现

```cpp
class GrpcBackend : public IAlgorithmBackend {
public:
    bool initialize(const AlgorithmConfig& config) override {
        // 创建 gRPC 发送器
        grpc_sender_ = std::make_unique<GrpcVideoSender>(
            config.grpc.server_address,
            config.grpc.target_fps
        );
        return grpc_sender_->connect();
    }
    
    void processFrame(const VideoFrame& frame) override {
        // 编码为 JPEG 并发送
        std::vector<uint8_t> jpeg_data;
        
        // 使用零拷贝接口（如果可用）
        if (jpeg_buffer_.empty()) {
            jpeg_buffer_.resize(500 * 1024);  // 500KB
        }
        
        size_t jpeg_size = yuv_to_jpeg_converter_.ConvertYuv420pZeroCopy(
            frame.data[0], frame.data[1], frame.data[2],
            frame.width, frame.height,
            jpeg_buffer_.data(),
            jpeg_buffer_.size()
        );
        
        if (jpeg_size > 0) {
            grpc_sender_->sendFrame(
                jpeg_buffer_.data(),
                jpeg_size,
                frame.width,
                frame.height,
                frame.pts
            );
        }
    }
    
    void processFrame(cv::Mat&& frame, int64_t pts) override {
        // 如果需要，也可以从 BGR 编码
        std::vector<uint8_t> jpeg_data;
        cv::imencode(".jpg", frame, jpeg_data, {cv::IMWRITE_JPEG_QUALITY, 85});
        
        grpc_sender_->sendFrame(
            jpeg_data.data(),
            jpeg_data.size(),
            frame.cols,
            frame.rows,
            pts
        );
    }
    
private:
    std::unique_ptr<GrpcVideoSender> grpc_sender_;
    YuvToJpegConverter yuv_to_jpeg_converter_;
    std::vector<uint8_t> jpeg_buffer_;  // 预分配缓冲区
    int channel_id_;
};
```

---

### 3. VideoPipeline 重构

#### 3.1 新的成员变量

```cpp
class VideoPipeline {
private:
    // ==================== 核心组件 ====================
    PipelineConfig config_;
    boost::asio::io_context& io_ctx_;
    
    std::unique_ptr<ZlmHttpFlvPuller> puller_;
    std::unique_ptr<FfmpegDecoder> decoder_;
    
    // ==================== 算法后端（策略模式）====================
    std::unique_ptr<IAlgorithmBackend> algorithm_backend_;
    
    // ==================== 预处理组件（可选）====================
    std::unique_ptr<OpenCVFormatConverter> preprocess_converter_;  // YUV → BGR
    std::unique_ptr<YuvToJpegConverter> jpeg_encoder_;             // YUV → JPEG
    
    // ==================== 队列 ====================
    std::shared_ptr<RawPacketQueue> raw_queue_;
    std::shared_ptr<FrameDataQueue> decoded_queue_;
    
    // ==================== 状态 ====================
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> frames_received_{0};
    std::atomic<uint64_t> frames_decoded_{0};
    
    // ==================== 回调 ====================
    using FrameOutputCallback = std::function<void(int, cv::Mat&&, int64_t)>;
    FrameOutputCallback output_callback_;
};
```

---

#### 3.2 初始化逻辑

```cpp
bool VideoPipeline::initialize() {
    // 1. 创建拉流器
    puller_ = std::make_unique<ZlmHttpFlvPuller>(io_ctx_);
    
    // 2. 创建解码器
    decoder_ = std::make_unique<FfmpegDecoder>();
    decoder_->setThreadCount(config_.decoder.decoder_threads);
    
    // 3. 根据配置创建算法后端
    std::string algo_type = config_.algorithm.getActiveAlgorithm();
    
    if (algo_type == "openvino") {
        algorithm_backend_ = std::make_unique<OpenVINOBackend>();
    } else if (algo_type == "opencv") {
        algorithm_backend_ = std::make_unique<OpenCVBackend>();
        
        // OpenCV 需要预处理转换器
        if (config_.preprocess.enable_preprocess) {
            preprocess_converter_ = std::make_unique<OpenCVFormatConverter>();
        }
    } else if (algo_type == "grpc") {
        algorithm_backend_ = std::make_unique<GrpcBackend>();
        
        // gRPC 需要 JPEG 编码器
        jpeg_encoder_ = std::make_unique<YuvToJpegConverter>(85);
    } else {
        LOG_MAIN_ERROR_AT("No algorithm backend configured");
        return false;
    }
    
    // 4. 初始化算法后端
    if (!algorithm_backend_->initialize(config_.algorithm)) {
        LOG_MAIN_ERROR_AT("Failed to initialize algorithm backend: {}", algo_type);
        return false;
    }
    
    // 5. 设置回调
    setupCallbacks();
    
    return true;
}
```

---

#### 3.3 帧处理逻辑

```cpp
void VideoPipeline::onFrameDecoded(VideoFrame&& frame) {
    frames_decoded_.fetch_add(1);
    
    std::string algo_type = config_.algorithm.getActiveAlgorithm();
    
    if (algo_type == "openvino") {
        // ✅ 路径 A: 直接传入 OpenVINO（零拷贝）
        algorithm_backend_->processFrame(frame);
        
    } else if (algo_type == "opencv") {
        // ✅ 路径 B: 预处理后传入 OpenCV
        if (preprocess_converter_) {
            // 应用预处理（resize, filter 等）
            cv::Mat bgr = preprocess_converter_->Process(
                frame.data[0], frame.data[1], frame.data[2],
                frame.width, frame.height,
                config_.preprocess.filters,
                config_.preprocess.target_width,
                config_.preprocess.target_height
            );
            
            algorithm_backend_->processFrame(std::move(bgr), frame.pts);
        } else {
            // 无预处理，直接转换
            algorithm_backend_->processFrame(frame);
        }
        
    } else if (algo_type == "grpc") {
        // ✅ 路径 C: 编码为 JPEG 并通过 gRPC 发送
        algorithm_backend_->processFrame(frame);
    }
    
    // 如果有输出回调，也调用
    if (output_callback_) {
        // 转换为 BGR（如果需要）
        cv::Mat bgr = yuv_to_bgr(frame);
        output_callback_(config_.channel_id, std::move(bgr), frame.pts);
    }
}
```

---

### 4. 配置映射

#### 4.1 路径 A: OpenVINO 配置示例

```cpp
auto config = PipelineConfig::createWithOpenVINO(
    "rtsp://camera/stream",
    "models/yolov5s.xml",
    "CPU"
);

// 额外配置
config.decoder.decoder_threads = 2;
config.preprocess.enable_preprocess = false;  // OpenVINO 内部处理
```

**数据流**：
```
Decoder(YUV) → TensorData(零拷贝) → OpenVINO → Result
```

---

#### 4.2 路径 B: OpenCV 配置示例

```cpp
auto config = PipelineConfig::createWithOpenCV(
    "rtsp://camera/stream",
    "face_detect",
    "models/haarcascade_frontalface.xml"
);

// 预处理配置
config.preprocess.enable_preprocess = true;
config.preprocess.filters = {"resize"};
config.preprocess.target_width = 640;
config.preprocess.target_height = 480;
```

**数据流**：
```
Decoder(YUV) → Preprocess(resize) → BGR → OpenCV → Result
```

---

#### 4.3 路径 C: gRPC 配置示例

```cpp
auto config = PipelineConfig::createWithGrpc(
    "rtsp://camera/stream",
    "localhost:50053",
    10  // FPS
);

// 可选预处理
config.preprocess.enable_preprocess = true;
config.preprocess.filters = {"resize"};
config.preprocess.target_width = 640;
```

**数据流**：
```
Decoder(YUV) → Preprocess(optional) → JPEG → gRPC → Python → Result
```

---

## 📋 实施计划

### Phase 1: 基础架构重构（1-2 天）

**任务**：
1. ✅ 定义 `IAlgorithmBackend` 接口
2. ✅ 重构 `VideoPipeline` 使用策略模式
3. ✅ 移除硬编码的 gRPC 逻辑
4. ✅ 添加算法后端工厂

**文件**：
- `include/videopipeline/i_algorithm_backend.h`（新建）
- `include/videopipeline/video_pipeline.h`（修改）
- `src/video_pipeline.cpp`（重构）

---

### Phase 2: OpenVINO 后端实现（1 天）

**任务**：
1. ✅ 实现 `OpenVINOBackend`
2. ✅ 集成零拷贝接口
3. ✅ 编写单元测试
4. ✅ 性能基准测试

**文件**：
- `include/videopipeline/backends/openvino_backend.h`（新建）
- `src/backends/openvino_backend.cpp`（新建）
- `test/videopipeline/test_openvino_backend.cpp`（新建）

---

### Phase 3: OpenCV 后端实现（1-2 天）

**任务**：
1. ✅ 实现 `OpenCVBackend`
2. ✅ 集成预处理模块
3. ✅ 实现常见算法（人脸、运动检测）
4. ✅ 编写单元测试

**文件**：
- `include/videopipeline/backends/opencv_backend.h`（新建）
- `src/backends/opencv_backend.cpp`（新建）
- `include/videopipeline/preprocessor.h`（可能需要新建）

---

### Phase 4: gRPC 后端优化（1 天）

**任务**：
1. ✅ 实现 `GrpcBackend`
2. ✅ 集成 `YuvToJpegConverter` 零拷贝接口
3. ✅ 添加缓冲池管理
4. ✅ 优化性能

**文件**：
- `include/videopipeline/backends/grpc_backend.h`（新建）
- `src/backends/grpc_backend.cpp`（新建）

---

### Phase 5: 集成测试与文档（1 天）

**任务**：
1. ✅ 端到端测试（三种路径）
2. ✅ 性能对比测试
3. ✅ 编写使用文档
4. ✅ 更新示例代码

**文件**：
- `test/videopipeline/test_all_paths.cpp`（新建）
- `docs/VIDEO_PIPELINE_REFACTORING.md`（新建）
- `docs/USAGE_EXAMPLES.md`（新建）

---

## 📊 性能预期

### 延迟对比

| 路径 | 延迟 | CPU 使用 | 适用场景 |
|------|------|---------|---------|
| **A: OpenVINO** | 2-5ms | 低 | 实时推理 |
| **B: OpenCV** | 8-12ms | 中 | 图像处理 |
| **C: gRPC** | 40-60ms | 高 | 远程算法 |

### 内存使用

| 路径 | 每帧内存 | 说明 |
|------|---------|------|
| **A: OpenVINO** | ~0 KB | 零拷贝 |
| **B: OpenCV** | ~3 MB | BGR 转换 |
| **C: gRPC** | ~500 KB | JPEG 缓冲 |

---

## ⚠️ 风险与挑战

### 1. 向后兼容性

**问题**：现有代码使用了旧的配置方式

**解决方案**：
- 提供迁移指南
- 保留旧 API 作为别名（deprecated）
- 逐步迁移

---

### 2. OpenVINO 模型兼容性

**问题**：不是所有模型都支持 YUV 输入

**解决方案**：
- 在 `OpenVINOBackend` 中添加格式检测
- 如果不支持 YUV，自动转换为 BGR
- 记录警告日志

---

### 3. 线程安全

**问题**：多个后端可能同时访问共享资源

**解决方案**：
- 每个 `VideoPipeline` 实例独立
- 使用原子操作统计计数
- 避免共享可变状态

---

### 4. 错误处理

**问题**：不同后端的错误类型不同

**解决方案**：
- 定义统一的错误码
- 详细的错误日志
- 优雅降级（fallback）

---

## 🎯 成功标准

### 功能标准

- ✅ 三种路径都能正常工作
- ✅ 可以动态切换算法后端
- ✅ 配置简单直观
- ✅ 错误处理完善

### 性能标准

- ✅ OpenVINO 路径延迟 < 5ms
- ✅ OpenCV 路径延迟 < 15ms
- ✅ gRPC 路径延迟 < 100ms
- ✅ 内存使用符合预期

### 代码质量标准

- ✅ 单元测试覆盖率 > 80%
- ✅ 代码审查通过
- ✅ 文档完整
- ✅ 示例代码可运行

---

## 📚 相关文档

- [PIPELINE_CONFIG_USAGE.md](./PIPELINE_CONFIG_USAGE.md) - 配置使用指南
- [ZERO_COPY_USAGE.md](../preprocess/docs/ZERO_COPY_USAGE.md) - 零拷贝接口使用
- [OPTIMIZATION_YUV_TO_JPEG.md](./OPTIMIZATION_YUV_TO_JPEG.md) - YUV→JPEG 优化

---

**创建日期**: 2026-05-04  
**作者**: Lingma AI Assistant  
**版本**: v1.0
