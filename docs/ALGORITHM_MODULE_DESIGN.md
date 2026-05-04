# C++ 算法模块详细设计文档

## 📚 目录

1. [模块架构](#模块架构)
2. [核心接口设计](#核心接口设计)
3. [预处理模块设计](#预处理模块设计)
4. [推理引擎模块设计](#推理引擎模块设计)
5. [后处理模块设计](#后处理模块设计)
6. [算法实现设计](#算法实现设计)
7. [处理器设计](#处理器设计)
8. [数据流设计](#数据流设计)
9. [性能优化策略](#性能优化策略)

---

## 模块架构

### 分层架构

```
┌─────────────────────────────────────────┐
│         Application Layer               │
│    (VideoPipeline, API Handlers)        │
└──────────────┬──────────────────────────┘
               │
┌──────────────▼──────────────────────────┐
│       Processor Layer                    │
│  (GrpcProcessor, NativeCppProcessor)    │
└──────────────┬──────────────────────────┘
               │
┌──────────────▼──────────────────────────┐
│       Algorithm Layer                    │
│   (YOLOv5Algorithm, MotionDetection)    │
└──┬──────────┬──────────────┬────────────┘
   │          │              │
┌──▼───┐ ┌───▼──────┐ ┌────▼────────┐
│Pre-  │ │Inference │ │Post-        │
│proc  │ │ Engine   │ │proc         │
└──────┘ └──────────┘ └─────────────┘
```

### 依赖关系

```
Application → Processor → Algorithm → [Preprocessor, InferenceEngine, Postprocessor]
                                         ↓
                                      Utils (Timer, Logger, Config)
```

---

## 核心接口设计

### 1. IAlgorithmProcessor（处理器接口）

**文件**: `modules/alg/include/alg/i_algorithm_processor.h`

```cpp
class IAlgorithmProcessor {
public:
    virtual ~IAlgorithmProcessor() = default;
    
    /// @brief 启动处理器
    virtual bool Start() = 0;
    
    /// @brief 停止处理器
    virtual void Stop() = 0;
    
    /// @brief 处理视频帧（异步）
    virtual bool ProcessFrame(const VideoFrame& frame) = 0;
    
    /// @brief 设置检测结果回调
    using DetectionCallback = std::function<void(const DetectionResult&)>;
    virtual void SetDetectionCallback(DetectionCallback callback) = 0;
    
    /// @brief 获取统计信息
    virtual ProcessorStats GetStats() const = 0;
    
    /// @brief 检查处理器是否可用
    virtual bool IsAvailable() const = 0;
    
    /// @brief 获取处理器类型
    virtual ProcessorType GetType() const = 0;
};
```

**设计要点**：
- ✅ 异步处理，不阻塞调用线程
- ✅ 回调机制通知结果
- ✅ 统计信息用于监控

---

### 2. IAlgorithm（算法接口）

**文件**: `modules/alg/include/alg/i_algorithm.h`

```cpp
class IAlgorithm {
public:
    virtual ~IAlgorithm() = default;
    
    /// @brief 初始化算法
    virtual bool Initialize(const AlgorithmConfig& config) = 0;
    
    /// @brief 处理单帧
    virtual AlgorithmResult Process(cv::Mat& frame, int64_t timestamp) = 0;
    
    /// @brief 获取算法名称
    virtual std::string GetName() const = 0;
    
    /// @brief 检查算法是否可用
    virtual bool IsAvailable() const = 0;
};
```

**设计要点**：
- ✅ 统一的算法接口
- ✅ 支持配置化初始化
- ✅ 返回结构化结果

---

### 3. IPreprocessor（预处理接口）

**文件**: `modules/alg/preprocess/include/alg/preprocess/i_preprocessor.h`

```cpp
struct PreprocessConfig {
    int target_width = 640;
    int target_height = 640;
    bool keep_aspect_ratio = true;
    float mean[3] = {0.0f, 0.0f, 0.0f};
    float std[3] = {1.0f, 1.0f, 1.0f};
    bool rgb_order = true;
    bool chw_format = true;
};

class IPreprocessor {
public:
    virtual ~IPreprocessor() = default;
    
    /// @brief 初始化预处理器
    virtual bool Initialize(const PreprocessConfig& config) = 0;
    
    /// @brief 预处理单帧
    /// @param input 输入图像（BGR, HWC）
    /// @param output 输出张量（RGB, CHW, 归一化）
    /// @param metadata 输出元数据（缩放比例、偏移等）
    virtual bool Process(const cv::Mat& input, 
                        std::vector<float>& output,
                        PreprocessMetadata& metadata) = 0;
    
    /// @brief 批量预处理
    virtual bool BatchProcess(const std::vector<cv::Mat>& inputs,
                             std::vector<std::vector<float>>& outputs,
                             std::vector<PreprocessMetadata>& metadatas) = 0;
};
```

**设计要点**：
- ✅ 支持单帧和批量处理
- ✅ 输出元数据用于后处理还原
- ✅ 配置灵活

---

### 4. IInferenceEngine（推理引擎接口）

**文件**: `modules/alg/inference/include/alg/inference/i_inference_engine.h`

```cpp
struct InferenceConfig {
    std::string model_path;
    std::string device = "CPU";  // CPU, GPU, MULTI:CPU,GPU
    bool async_mode = true;
    int num_requests = 4;
    int batch_size = 1;
};

struct TensorInfo {
    std::string name;
    std::vector<int64_t> shape;
    void* data = nullptr;
    size_t size = 0;
};

class IInferenceEngine {
public:
    virtual ~IInferenceEngine() = default;
    
    /// @brief 加载模型
    virtual bool LoadModel(const InferenceConfig& config) = 0;
    
    /// @brief 同步推理
    virtual bool Infer(const std::vector<TensorInfo>& inputs,
                      std::vector<TensorInfo>& outputs) = 0;
    
    /// @brief 异步推理
    virtual bool InferAsync(const std::vector<TensorInfo>& inputs,
                           std::function<void(const std::vector<TensorInfo>&)> callback) = 0;
    
    /// @brief 等待异步推理完成
    virtual bool WaitAsync() = 0;
    
    /// @brief 获取输入/输出信息
    virtual std::vector<TensorInfo> GetInputInfo() const = 0;
    virtual std::vector<TensorInfo> GetOutputInfo() const = 0;
};
```

**设计要点**：
- ✅ 支持同步和异步推理
- ✅ 抽象张量接口（适配不同引擎）
- ✅ 异步回调机制

---

### 5. IPostprocessor（后处理接口）

**文件**: `modules/alg/postprocess/include/alg/postprocess/i_postprocessor.h`

```cpp
struct PostprocessConfig {
    float conf_threshold = 0.25f;
    float iou_threshold = 0.45f;
    int max_detections = 100;
    std::vector<std::string> class_names;
};

class IPostprocessor {
public:
    virtual ~IPostprocessor() = default;
    
    /// @brief 初始化后处理器
    virtual bool Initialize(const PostprocessConfig& config) = 0;
    
    /// @brief 后处理
    /// @param raw_output 原始推理输出
    /// @param metadata 预处理元数据
    /// @return 检测结果
    virtual DetectionResult Process(const std::vector<float>& raw_output,
                                   const PreprocessMetadata& metadata) = 0;
    
    /// @brief 批量后处理
    virtual std::vector<DetectionResult> BatchProcess(
        const std::vector<std::vector<float>>& raw_outputs,
        const std::vector<PreprocessMetadata>& metadatas) = 0;
};
```

**设计要点**：
- ✅ 使用预处理元数据还原坐标
- ✅ 支持批量处理
- ✅ 配置灵活

---

## 预处理模块设计

### Letterbox 缩放算法

**文件**: `modules/alg/preprocess/src/letterbox.cpp`

```cpp
class LetterboxPreprocessor : public IPreprocessor {
private:
    int target_w_, target_h_;
    float scale_ = 1.0f;
    int pad_w_ = 0, pad_h_ = 0;
    
public:
    bool Process(const cv::Mat& input, 
                std::vector<float>& output,
                PreprocessMetadata& metadata) override {
        
        // 1. 计算缩放比例（保持宽高比）
        float ratio_w = static_cast<float>(target_w_) / input.cols;
        float ratio_h = static_cast<float>(target_h_) / input.rows;
        scale_ = std::min(ratio_w, ratio_h);
        
        // 2. 计算缩放后的尺寸
        int new_w = static_cast<int>(input.cols * scale_);
        int new_h = static_cast<int>(input.rows * scale_);
        
        // 3. Resize
        cv::Mat resized;
        cv::resize(input, resized, cv::Size(new_w, new_h), 
                  0, 0, cv::INTER_LINEAR);
        
        // 4. 计算填充
        pad_w_ = (target_w_ - new_w) / 2;
        pad_h_ = (target_h_ - new_h) / 2;
        
        // 5. 创建目标图像并填充
        cv::Mat padded(target_h_, target_w_, CV_8UC3, cv::Scalar(114, 114, 114));
        resized.copyTo(padded(cv::Rect(pad_w_, pad_h_, new_w, new_h)));
        
        // 6. 保存元数据
        metadata.scale = scale_;
        metadata.pad_w = pad_w_;
        metadata.pad_h = pad_h_;
        metadata.original_w = input.cols;
        metadata.original_h = input.rows;
        
        // 7. 后续处理（归一化、格式转换）...
        
        return true;
    }
};
```

**关键点**：
- ✅ 保持宽高比
- ✅ 灰色填充（114, 114, 114）
- ✅ 记录缩放比例和偏移

---

## 推理引擎模块设计

### OpenVINO 引擎实现

**文件**: `modules/alg/inference/src/openvino_engine.cpp`

```cpp
#include <openvino/openvino.hpp>

class OpenVINOEngine : public IInferenceEngine {
private:
    ov::Core core_;
    ov::CompiledModel compiled_model_;
    std::vector<ov::InferRequest> infer_requests_;
    int current_request_idx_ = 0;
    bool async_mode_ = false;
    
public:
    bool LoadModel(const InferenceConfig& config) override {
        // 1. 读取模型
        auto model = core_.read_model(config.model_path);
        
        // 2. 配置设备
        ov::AnyMap device_config;
        if (config.device == "GPU") {
            device_config["GPU_THROUGHPUT_STREAMS"] = "AUTO";
        }
        
        // 3. 编译模型
        compiled_model_ = core_.compile_model(
            model, config.device, device_config);
        
        // 4. 创建推理请求池
        if (config.async_mode) {
            for (int i = 0; i < config.num_requests; ++i) {
                infer_requests_.push_back(compiled_model_.create_infer_request());
            }
            async_mode_ = true;
        }
        
        return true;
    }
    
    bool InferAsync(const std::vector<TensorInfo>& inputs,
                   std::function<void(const std::vector<TensorInfo>&)> callback) override {
        
        // 1. 获取空闲的推理请求
        auto& request = infer_requests_[current_request_idx_];
        current_request_idx_ = (current_request_idx_ + 1) % infer_requests_.size();
        
        // 2. 设置输入
        for (const auto& input : inputs) {
            auto ov_tensor = ov::Tensor(
                ov::element::f32,
                ov::Shape(input.shape.begin(), input.shape.end()),
                input.data
            );
            request.set_input_tensor(input.name, ov_tensor);
        }
        
        // 3. 设置回调
        request.set_callback([this, callback]() {
            // 获取输出
            std::vector<TensorInfo> outputs;
            // ... 提取输出张量
            
            callback(outputs);
        });
        
        // 4. 启动异步推理
        request.start_async();
        
        return true;
    }
};
```

**关键点**：
- ✅ 推理请求池（多请求并发）
- ✅ 异步回调机制
- ✅ 设备配置灵活

---

## 后处理模块设计

### NMS 实现

**文件**: `modules/alg/postprocess/src/nms.cpp`

```cpp
class NMSProcessor {
public:
    /// @brief 标准 NMS
    static std::vector<int> ApplyNMS(
        const std::vector<BoundingBox>& boxes,
        float iou_threshold) {
        
        // 1. 按置信度排序
        std::vector<int> indices(boxes.size());
        std::iota(indices.begin(), indices.end(), 0);
        std::sort(indices.begin(), indices.end(), 
                 [&boxes](int a, int b) {
                     return boxes[a].confidence > boxes[b].confidence;
                 });
        
        // 2. NMS
        std::vector<bool> suppressed(boxes.size(), false);
        std::vector<int> keep_indices;
        
        for (size_t i = 0; i < indices.size(); ++i) {
            if (suppressed[indices[i]]) continue;
            
            keep_indices.push_back(indices[i]);
            
            for (size_t j = i + 1; j < indices.size(); ++j) {
                if (suppressed[indices[j]]) continue;
                
                float iou = CalculateIoU(boxes[indices[i]], boxes[indices[j]]);
                if (iou > iou_threshold) {
                    suppressed[indices[j]] = true;
                }
            }
        }
        
        return keep_indices;
    }
    
private:
    static float CalculateIoU(const BoundingBox& a, const BoundingBox& b) {
        float x1 = std::max(a.x, b.x);
        float y1 = std::max(a.y, b.y);
        float x2 = std::min(a.x + a.width, b.x + b.width);
        float y2 = std::min(a.y + a.height, b.y + b.height);
        
        float intersection = std::max(0.0f, x2 - x1) * std::max(0.0f, y2 - y1);
        float area_a = a.width * a.height;
        float area_b = b.width * b.height;
        
        return intersection / (area_a + area_b - intersection);
    }
};
```

**优化方向**：
- ⚡ SIMD 优化（AVX2）
- ⚡ 并行化处理
- ⚡ Soft-NMS 支持

---

## 算法实现设计

### YOLOv5Algorithm

**文件**: `modules/alg/algorithms/src/yolov5_algorithm.cpp`

```cpp
class YOLOv5Algorithm : public IAlgorithm {
private:
    std::unique_ptr<IPreprocessor> preprocessor_;
    std::unique_ptr<IInferenceEngine> engine_;
    std::unique_ptr<IPostprocessor> postprocessor_;
    
    AlgorithmConfig config_;
    bool initialized_ = false;
    
public:
    bool Initialize(const AlgorithmConfig& config) override {
        config_ = config;
        
        // 1. 初始化预处理器
        PreprocessConfig preprocess_cfg;
        preprocess_cfg.target_width = 640;
        preprocess_cfg.target_height = 640;
        // ...
        preprocessor_ = std::make_unique<LetterboxPreprocessor>();
        preprocessor_->Initialize(preprocess_cfg);
        
        // 2. 初始化推理引擎
        InferenceConfig inference_cfg;
        inference_cfg.model_path = config.model_path;
        inference_cfg.device = config.device;
        inference_cfg.async_mode = true;
        inference_cfg.num_requests = 4;
        
        engine_ = std::make_unique<OpenVINOEngine>();
        engine_->LoadModel(inference_cfg);
        
        // 3. 初始化后处理器
        PostprocessConfig postprocess_cfg;
        postprocess_cfg.conf_threshold = config.conf_threshold;
        postprocess_cfg.iou_threshold = config.iou_threshold;
        // ...
        postprocessor_ = std::make_unique<YOLOv5Postprocessor>();
        postprocessor_->Initialize(postprocess_cfg);
        
        initialized_ = true;
        return true;
    }
    
    AlgorithmResult Process(cv::Mat& frame, int64_t timestamp) override {
        Timer timer;
        timer.Start();
        
        // 1. 预处理
        std::vector<float> input_tensor;
        PreprocessMetadata metadata;
        preprocessor_->Process(frame, input_tensor, metadata);
        
        // 2. 推理
        TensorInfo input_info;
        input_info.data = input_tensor.data();
        input_info.shape = {1, 3, 640, 640};
        
        std::vector<TensorInfo> outputs;
        engine_->Infer({input_info}, outputs);
        
        // 3. 后处理
        DetectionResult detection = postprocessor_->Process(
            outputs[0], metadata);
        
        timer.Stop();
        
        // 4. 组装结果
        AlgorithmResult result;
        result.timestamp_us = timestamp;
        result.algorithm_type = "yolov5";
        result.processing_time_ms = timer.ElapsedMs();
        result.detection_result = detection;
        
        return result;
    }
};
```

**设计要点**：
- ✅ 组合模式（预处理+推理+后处理）
- ✅ 性能计时
- ✅ 配置驱动

---

## 处理器设计

### NativeCppProcessor

**文件**: `modules/alg/processors/src/native_cpp_processor.cpp`

```cpp
class NativeCppProcessor : public IAlgorithmProcessor {
private:
    std::unique_ptr<IAlgorithm> algorithm_;
    DetectionCallback callback_;
    ProcessorStats stats_;
    std::atomic<bool> running_{false};
    
    // 异步处理队列
    std::queue<VideoFrame> frame_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::thread worker_thread_;
    
public:
    bool Start() override {
        running_ = true;
        worker_thread_ = std::thread(&NativeCppProcessor::WorkerLoop, this);
        return true;
    }
    
    void Stop() override {
        running_ = false;
        queue_cv_.notify_one();
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }
    
    bool ProcessFrame(const VideoFrame& frame) override {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        frame_queue_.push(frame);
        queue_cv_.notify_one();
        return true;
    }
    
private:
    void WorkerLoop() {
        while (running_) {
            VideoFrame frame;
            
            // 从队列获取帧
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                queue_cv_.wait(lock, [this] {
                    return !frame_queue_.empty() || !running_;
                });
                
                if (!running_) break;
                
                frame = std::move(frame_queue_.front());
                frame_queue_.pop();
            }
            
            // 解码 JPEG → cv::Mat
            cv::Mat image = DecodeJPEG(frame.data);
            
            // 处理帧
            auto result = algorithm_->Process(image, frame.timestamp);
            
            // 更新统计
            UpdateStats(result.processing_time_ms);
            
            // 回调通知
            if (callback_) {
                callback_(result.detection_result);
            }
        }
    }
};
```

**设计要点**：
- ✅ 生产者-消费者模式
- ✅ 工作线程异步处理
- ✅ 统计信息收集

---

## 数据流设计

### 完整处理流程

```
┌─────────────┐
│ VideoFrame  │  (JPEG 编码)
└──────┬──────┘
       │ Decode
       ▼
┌─────────────┐
│  cv::Mat    │  (BGR, HWC)
└──────┬──────┘
       │ Preprocess
       ▼
┌─────────────┐
│ Input Tensor│  (RGB, CHW, Normalized)
└──────┬──────┘
       │ Infer
       ▼
┌─────────────┐
│Output Tensor│  (Raw predictions)
└──────┬──────┘
       │ Postprocess
       ▼
┌─────────────┐
│DetectionResult│ (BoundingBoxes)
└──────┬──────┘
       │ Callback
       ▼
┌─────────────┐
│ Application │
└─────────────┘
```

### 内存管理策略

1. **零拷贝优化**：
   - OpenVINO 直接使用输入缓冲区
   - 避免额外的内存复制

2. **内存池**：
   - 预分配张量缓冲区
   - 复用 cv::Mat 对象

3. **智能指针**：
   - `std::unique_ptr` 管理生命周期
   - `std::shared_ptr` 共享所有权

---

## 性能优化策略

### 1. 流水线并行

```
Thread 1: [Preprocess Frame N] → [Infer Frame N-1] → [Postprocess Frame N-2]
Thread 2: [Preprocess Frame N+1] → [Infer Frame N] → [Postprocess Frame N-1]
```

### 2. 批处理

```cpp
// 累积多帧后批量处理
if (batch.size() >= batch_size) {
    preprocessor->BatchProcess(batch, ...);
    engine->InferBatch(...);
    postprocessor->BatchProcess(...);
}
```

### 3. SIMD 优化

```cpp
// 使用 OpenCV 的 SIMD 优化函数
cv::resize(..., cv::INTER_LINEAR);  // 自动使用 SSE/AVX
cv::cvtColor(...);                   // 自动优化

// 自定义 NMS 使用 AVX2
#ifdef __AVX2__
    // AVX2 优化的 NMS 实现
#endif
```

### 4. 异步推理

```cpp
// 4 个推理请求并发
Request 0: [Infer Frame 0]
Request 1: [Infer Frame 1]
Request 2: [Infer Frame 2]
Request 3: [Infer Frame 3]
```

### 5. 缓存友好

```cpp
// 结构体对齐
struct alignas(64) BoundingBox {
    float x, y, width, height;
    float confidence;
    int class_id;
};

// 连续内存布局
std::vector<BoundingBox> boxes;  // 而非 std::vector<std::unique_ptr<BoundingBox>>
```

---

## 错误处理设计

### 异常安全

```cpp
bool ProcessFrame(const VideoFrame& frame) {
    try {
        // 预处理
        auto tensor = preprocessor_->Process(frame);
        
        // 推理
        auto output = engine_->Infer(tensor);
        
        // 后处理
        auto result = postprocessor_->Process(output);
        
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Processing failed: {}", e.what());
        stats_.frames_failed++;
        return false;
    }
}
```

### 优雅降级

```cpp
if (!engine_->IsAvailable()) {
    LOG_WARN("Inference engine unavailable, falling back to CPU");
    config_.device = "CPU";
    engine_->Reload(config_);
}
```

---

## 测试策略

### 单元测试

```cpp
TEST(LetterboxTest, KeepAspectRatio) {
    cv::Mat input(480, 640, CV_8UC3);
    std::vector<float> output;
    PreprocessMetadata metadata;
    
    LetterboxPreprocessor preprocessor;
    preprocessor.Initialize({640, 640, true});
    preprocessor.Process(input, output, metadata);
    
    EXPECT_FLOAT_EQ(metadata.scale, 1.0f);
    EXPECT_EQ(output.size(), 640 * 640 * 3);
}
```

### 集成测试

```cpp
TEST(YOLOv5AlgorithmTest, EndToEnd) {
    YOLOv5Algorithm algo;
    algo.Initialize(config);
    
    cv::Mat frame = cv::imread("test.jpg");
    auto result = algo.Process(frame, 0);
    
    EXPECT_GT(result.detection_result.boxes.size(), 0);
    EXPECT_GT(result.processing_time_ms, 0);
}
```

### 性能基准

```cpp
BENCHMARK(YOLOv5Inference) {
    for (auto _ : state) {
        auto result = algo.Process(frame, 0);
    }
}
```

---

**文档版本**: v1.0  
**最后更新**: 2026-04-29  
**作者**: Lingma AI Assistant
