# VideoPipeline 多方案零拷贝接口设计

## 📋 设计目标

1. **多方案支持**：同时支持 CPU 和 GPU 解码、推理
2. **最小化拷贝**：通过内存池、零拷贝解析、GPU 直通等技术减少数据拷贝
3. **灵活切换**：运行时可配置选择不同方案
4. **统一接口**：上层应用无需关心底层实现细节

---

## 🏗️ 整体架构

### 流水线阶段

```
┌─────────┐    ┌──────────┐    ┌────────────┐    ┌──────────┐    ┌───────────┐
│ Puller  │───▶│ Decoder  │───▶│Preprocessor│───▶│Algorithm │───▶│Postprocess│
│ (拉流)   │    │ (解码)    │    │ (预处理)    │    │ (推理)    │    │ (后处理)   │
└─────────┘    └──────────┘    └────────────┘    └──────────┘    └───────────┘
     │              │                 │                 │                │
     ▼              ▼                 ▼                 ▼                ▼
  RawPacket    FrameData        TensorData       DetectionResult   FinalResult
```

### 关键设计原则

1. **接口抽象**：每个阶段定义抽象接口，支持多种实现
2. **资源管理**：统一的内存/显存池管理
3. **零拷贝传递**：使用视图（View）、智能指针、移动语义
4. **异步流水线**：支持多帧并发处理

---

## 🔌 核心接口设计

### 1. IPuller（拉流器接口）

**文件**: `modules/videopipeline/include/videopipeline/puller/i_puller.h`

```cpp
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

/// @brief NALU 数据视图（零拷贝）
struct NaluView {
    const uint8_t* data;  ///< 指向原始缓冲区的指针（不拥有所有权）
    int size;             ///< NALU 大小
    int64_t pts;          ///< 时间戳
    bool is_keyframe;     ///< 是否关键帧
    
    NaluView() : data(nullptr), size(0), pts(0), is_keyframe(false) {}
    NaluView(const uint8_t* d, int s, int64_t t, bool kf) 
        : data(d), size(s), pts(t), is_keyframe(kf) {}
};

/// @brief 拉流器回调类型
using NaluCallback = std::function<void(const NaluView& nalu)>;

/// @brief 拉流器配置
struct PullerConfig {
    std::string url;              ///< 流地址
    int reconnect_delay = 3;      ///< 重连延迟（秒）
    int max_reconnect_attempts = -1; ///< 最大重连次数（-1=无限）
};

/// @brief 拉流器接口
class IPuller {
public:
    virtual ~IPuller() = default;
    
    /// @brief 初始化拉流器
    virtual bool Initialize(const PullerConfig& config) = 0;
    
    /// @brief 启动拉流
    virtual bool Start() = 0;
    
    /// @brief 停止拉流
    virtual void Stop() = 0;
    
    /// @brief 设置 NALU 回调
    virtual void SetNaluCallback(NaluCallback callback) = 0;
    
    /// @brief 检查是否正在运行
    virtual bool IsRunning() const = 0;
    
    /// @brief 获取统计信息
    struct Stats {
        uint64_t bytes_received = 0;
        uint64_t nalus_received = 0;
        uint64_t keyframes_received = 0;
    };
    virtual Stats GetStats() const = 0;
};
```

**设计要点**：
- ✅ `NaluView` 使用指针视图，避免拷贝 H.264 数据
- ✅ 回调机制异步通知，不阻塞拉流线程
- ✅ 统计信息用于监控

---

### 2. IDecoder（解码器接口）

**文件**: `modules/videopipeline/include/videopipeline/decoder/i_decoder.h`

```cpp
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <opencv2/opencv.hpp>

// 前向声明
namespace cv {
    class Mat;
}

/// @brief 解码器类型枚举
enum class DecoderType {
    FFMPEG_CPU,      ///< FFmpeg CPU 软解码
    NVDEC_GPU,       ///< NVIDIA NVDEC 硬解码
    VAAPI_LINUX,     ///< Linux VAAPI
    VIDEO_TOOLBOX_MAC ///< macOS VideoToolbox
};

/// @brief 解码帧数据结构（支持 CPU/GPU）
struct DecodedFrame {
    int channel_id = -1;
    int64_t pts = 0;
    int64_t dts = 0;
    
    // CPU 路径：OpenCV Mat
    cv::Mat cpu_frame;  ///< CPU 帧数据（BGR）
    
    // GPU 路径：GPU 句柄
    void* gpu_handle = nullptr;  ///< GPU 帧句柄（CUvideoframe / VAImage 等）
    int gpu_device_id = -1;      ///< GPU 设备 ID
    
    int width = 0;
    int height = 0;
    bool is_gpu = false;  ///< 是否在 GPU 上
    
    DecodedFrame() = default;
    
    /// @brief 移动构造函数
    DecodedFrame(DecodedFrame&& other) noexcept
        : channel_id(other.channel_id)
        , pts(other.pts)
        , dts(other.dts)
        , cpu_frame(std::move(other.cpu_frame))
        , gpu_handle(other.gpu_handle)
        , gpu_device_id(other.gpu_device_id)
        , width(other.width)
        , height(other.height)
        , is_gpu(other.is_gpu) {
        other.gpu_handle = nullptr;
        other.is_gpu = false;
    }
    
    /// @brief 移动赋值
    DecodedFrame& operator=(DecodedFrame&& other) noexcept {
        if (this != &other) {
            channel_id = other.channel_id;
            pts = other.pts;
            dts = other.dts;
            cpu_frame = std::move(other.cpu_frame);
            gpu_handle = other.gpu_handle;
            gpu_device_id = other.gpu_device_id;
            width = other.width;
            height = other.height;
            is_gpu = other.is_gpu;
            other.gpu_handle = nullptr;
            other.is_gpu = false;
        }
        return *this;
    }
    
    /// @brief 释放资源
    void Release() {
        cpu_frame.release();
        gpu_handle = nullptr;
        is_gpu = false;
    }
};

/// @brief 解码器回调类型
using FrameCallback = std::function<void(DecodedFrame&& frame)>;

/// @brief 解码器配置
struct DecoderConfig {
    DecoderType type = DecoderType::FFMPEG_CPU;
    int codec_id = 7;  ///< 7=H.264, 12=H.265
    int gpu_device_id = 0;  ///< GPU 设备 ID
    bool async_mode = true; ///< 异步解码
    int num_decode_threads = 4; ///< 解码线程数（CPU）
};

/// @brief 解码器接口
class IDecoder {
public:
    virtual ~IDecoder() = default;
    
    /// @brief 初始化解码器
    virtual bool Initialize(const DecoderConfig& config) = 0;
    
    /// @brief 提供 SPS/PPS 数据（用于初始化解码器）
    virtual bool SetSequenceHeader(const uint8_t* data, int size) = 0;
    
    /// @brief 解码单个 NALU
    virtual bool DecodeNalu(const uint8_t* data, int size, int64_t pts) = 0;
    
    /// @brief 批量解码 NALUs
    virtual bool DecodeBatch(const std::vector<NaluView>& nalus) = 0;
    
    /// @brief 设置帧回调
    virtual void SetFrameCallback(FrameCallback callback) = 0;
    
    /// @brief 获取解码器类型
    virtual DecoderType GetType() const = 0;
    
    /// @brief 检查解码器是否可用
    virtual bool IsAvailable() const = 0;
    
    /// @brief 获取统计信息
    struct Stats {
        uint64_t frames_decoded = 0;
        uint64_t decode_errors = 0;
        double avg_decode_time_ms = 0.0;
    };
    virtual Stats GetStats() const = 0;
};
```

**设计要点**：
- ✅ `DecodedFrame` 同时支持 CPU (`cv::Mat`) 和 GPU (`gpu_handle`)
- ✅ 移动语义避免帧数据拷贝
- ✅ 支持同步和异步解码
- ✅ 统一的统计接口

---

### 3. IPreprocessor（预处理器接口）

**文件**: `modules/alg/preprocess/include/alg/preprocess/i_preprocessor.h`

```cpp
#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <opencv2/opencv.hpp>

/// @brief 预处理设备类型
enum class PreprocessDevice {
    CPU,   ///< CPU 预处理
    CUDA,  ///< CUDA 预处理
    OPENCL ///< OpenCL 预处理
};

/// @brief 预处理元数据
struct PreprocessMetadata {
    float scale = 1.0f;         ///< 缩放比例
    int pad_w = 0;              ///< 宽度填充
    int pad_h = 0;              ///< 高度填充
    int original_w = 0;         ///< 原始宽度
    int original_h = 0;         ///< 原始高度
    
    // GPU 特定
    void* cuda_stream = nullptr; ///< CUDA 流（可选）
};

/// @brief 张量数据（支持 CPU/GPU）
struct TensorData {
    void* data = nullptr;       ///< 数据指针（CPU 或 GPU）
    std::vector<int64_t> shape; ///< 形状 [N, C, H, W]
    bool is_gpu = false;        ///< 是否在 GPU 上
    size_t size_bytes = 0;      ///< 数据大小（字节）
    
    TensorData() = default;
    
    /// @brief CPU 张量
    static TensorData FromCpu(const std::vector<float>& data, 
                             const std::vector<int64_t>& shape) {
        TensorData tensor;
        tensor.data = const_cast<float*>(data.data());
        tensor.shape = shape;
        tensor.is_gpu = false;
        tensor.size_bytes = data.size() * sizeof(float);
        return tensor;
    }
    
    /// @brief GPU 张量
    static TensorData FromGpu(void* gpu_ptr, 
                             const std::vector<int64_t>& shape,
                             size_t size_bytes) {
        TensorData tensor;
        tensor.data = gpu_ptr;
        tensor.shape = shape;
        tensor.is_gpu = true;
        tensor.size_bytes = size_bytes;
        return tensor;
    }
};

/// @brief 预处理配置
struct PreprocessConfig {
    PreprocessDevice device = PreprocessDevice::CPU;
    int target_width = 640;
    int target_height = 640;
    bool keep_aspect_ratio = true;
    float mean[3] = {0.0f, 0.0f, 0.0f};
    float std[3] = {1.0f, 1.0f, 1.0f};
    bool rgb_order = true;
    bool chw_format = true;
    int gpu_device_id = 0;
};

/// @brief 预处理器接口
class IPreprocessor {
public:
    virtual ~IPreprocessor() = default;
    
    /// @brief 初始化预处理器
    virtual bool Initialize(const PreprocessConfig& config) = 0;
    
    /// @brief 预处理单帧（CPU 路径）
    virtual bool Process(const cv::Mat& input, 
                        TensorData& output,
                        PreprocessMetadata& metadata) = 0;
    
    /// @brief 预处理单帧（GPU 路径，输入为 YUV GPU 帧）
    virtual bool ProcessGpu(void* gpu_yuv_frame,
                           TensorData& output,
                           PreprocessMetadata& metadata,
                           void* cuda_stream = nullptr) = 0;
    
    /// @brief 批量预处理
    virtual bool BatchProcess(const std::vector<cv::Mat>& inputs,
                             std::vector<TensorData>& outputs,
                             std::vector<PreprocessMetadata>& metadatas) = 0;
    
    /// @brief 获取设备类型
    virtual PreprocessDevice GetDevice() const = 0;
};
```

**设计要点**：
- ✅ 同时支持 CPU 和 GPU 预处理
- ✅ `TensorData` 抽象 CPU/GPU 内存
- ✅ 元数据记录缩放信息，用于后处理还原
- ✅ 支持 CUDA 流，实现异步处理

---

### 4. IInferenceEngine（推理引擎接口）

**文件**: `modules/alg/inference/include/alg/inference/i_inference_engine.h`

```cpp
#pragma once

#include <cstdint>
#include <vector>
#include <functional>
#include <memory>
#include <map>
#include <string>

// 前向声明
struct TensorData;

/// @brief 推理引擎类型
enum class InferenceEngineType {
    OPENVINO_CPU,      ///< OpenVINO CPU
    OPENVINO_GPU,      ///< OpenVINO GPU
    TENSORRT,          ///< NVIDIA TensorRT
    ONNXRUNTIME_CPU,   ///< ONNX Runtime CPU
    ONNXRUNTIME_CUDA,  ///< ONNX Runtime CUDA
    COREML             ///< Apple CoreML
};

/// @brief 推理配置
struct InferenceConfig {
    InferenceEngineType type = InferenceEngineType::OPENVINO_CPU;
    std::string model_path;
    std::string device = "CPU";  ///< CPU, GPU, MULTI:CPU,GPU
    bool async_mode = true;
    int num_requests = 4;
    int batch_size = 1;
    int gpu_device_id = 0;
    
    // TensorRT 特定
    int max_workspace_size_mb = 512;
    bool fp16_mode = false;
    bool int8_mode = false;
};

/// @brief 推理结果
struct InferenceOutput {
    std::map<std::string, TensorData> tensors; ///< 输出张量
    int64_t inference_time_us = 0;             ///< 推理耗时（微秒）
    bool success = false;
    std::string error_message;
};

/// @brief 推理完成回调
using InferenceCallback = std::function<void(const InferenceOutput& output)>;

/// @brief 推理引擎接口
class IInferenceEngine {
public:
    virtual ~IInferenceEngine() = default;
    
    /// @brief 加载模型
    virtual bool LoadModel(const InferenceConfig& config) = 0;
    
    /// @brief 同步推理
    virtual InferenceOutput Infer(const TensorData& input) = 0;
    
    /// @brief 异步推理
    virtual bool InferAsync(const TensorData& input, 
                           InferenceCallback callback) = 0;
    
    /// @brief 批量推理
    virtual std::vector<InferenceOutput> InferBatch(
        const std::vector<TensorData>& inputs) = 0;
    
    /// @brief 等待所有异步推理完成
    virtual bool WaitAll() = 0;
    
    /// @brief 获取输入/输出信息
    struct TensorInfo {
        std::string name;
        std::vector<int64_t> shape;
        std::string dtype;  ///< FP32, INT8, etc.
    };
    virtual std::vector<TensorInfo> GetInputInfo() const = 0;
    virtual std::vector<TensorInfo> GetOutputInfo() const = 0;
    
    /// @brief 获取引擎类型
    virtual InferenceEngineType GetType() const = 0;
    
    /// @brief 检查引擎是否可用
    virtual bool IsAvailable() const = 0;
    
    /// @brief 获取统计信息
    struct Stats {
        uint64_t inferences_count = 0;
        uint64_t errors_count = 0;
        double avg_inference_time_ms = 0.0;
        double fps = 0.0;
    };
    virtual Stats GetStats() const = 0;
};
```

**设计要点**：
- ✅ 支持多种推理引擎（OpenVINO、TensorRT、ONNX Runtime）
- ✅ 同步和异步推理
- ✅ 批量推理支持
- ✅ 统一的张量接口

---

### 5. IPostprocessor（后处理器接口）

**文件**: `modules/alg/postprocess/include/alg/postprocess/i_postprocessor.h`

```cpp
#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <map>

// 前向声明
struct TensorData;
struct PreprocessMetadata;

/// @brief 检测框
struct BoundingBox {
    float x = 0.0f;         ///< 左上角 X
    float y = 0.0f;         ///< 左上角 Y
    float width = 0.0f;     ///< 宽度
    float height = 0.0f;    ///< 高度
    std::string class_name; ///< 类别名称
    float confidence = 0.0f;///< 置信度
    int class_id = 0;       ///< 类别 ID
    
    /// @brief 还原到原始图像坐标
    BoundingBox RestoreToOriginal(const PreprocessMetadata& metadata) const {
        BoundingBox restored;
        restored.x = (x - metadata.pad_w) / metadata.scale;
        restored.y = (y - metadata.pad_h) / metadata.scale;
        restored.width = width / metadata.scale;
        restored.height = height / metadata.scale;
        restored.class_name = class_name;
        restored.confidence = confidence;
        restored.class_id = class_id;
        return restored;
    }
};

/// @brief 检测结果
struct DetectionResult {
    std::string frame_id;
    std::vector<BoundingBox> boxes;
    int64_t processing_time_ms = 0;
    std::string algorithm;
    std::map<std::string, std::string> metadata;
};

/// @brief 后处理配置
struct PostprocessConfig {
    float conf_threshold = 0.25f;
    float iou_threshold = 0.45f;
    int max_detections = 100;
    std::vector<std::string> class_names;
    bool use_nms = true;
    bool restore_coordinates = true; ///< 是否还原到原始坐标
};

/// @brief 后处理器接口
class IPostprocessor {
public:
    virtual ~IPostprocessor() = default;
    
    /// @brief 初始化后处理器
    virtual bool Initialize(const PostprocessConfig& config) = 0;
    
    /// @brief 后处理
    virtual DetectionResult Process(const TensorData& raw_output,
                                   const PreprocessMetadata& metadata) = 0;
    
    /// @brief 批量后处理
    virtual std::vector<DetectionResult> BatchProcess(
        const std::vector<TensorData>& raw_outputs,
        const std::vector<PreprocessMetadata>& metadatas) = 0;
};
```

**设计要点**：
- ✅ `BoundingBox::RestoreToOriginal()` 自动还原坐标
- ✅ 支持 NMS 和置信度过滤
- ✅ 批量后处理

---

### 6. IAlgorithm（算法接口）

**文件**: `modules/alg/include/alg/i_algorithm.h`

```cpp
#pragma once

#include <memory>
#include <string>
#include <opencv2/opencv.hpp>

// 前向声明
struct DecodedFrame;
struct DetectionResult;
class IPreprocessor;
class IInferenceEngine;
class IPostprocessor;

/// @brief 算法配置
struct AlgorithmConfig {
    std::string name;
    std::string model_path;
    std::string device = "CPU";  ///< CPU, GPU
    float conf_threshold = 0.25f;
    float iou_threshold = 0.45f;
    
    // 预处理配置
    int input_width = 640;
    int input_height = 640;
    
    // 推理配置
    int num_requests = 4;
    bool async_mode = true;
};

/// @brief 算法结果
struct AlgorithmResult {
    int channel_id = -1;
    int64_t timestamp_us = 0;
    std::string algorithm_type;
    DetectionResult detection;
    float confidence = 0.0f;
    int64_t processing_time_ms = 0;
};

/// @brief 算法接口
class IAlgorithm {
public:
    virtual ~IAlgorithm() = default;
    
    /// @brief 初始化算法
    virtual bool Initialize(const AlgorithmConfig& config) = 0;
    
    /// @brief 处理单帧（同步）
    virtual AlgorithmResult Process(DecodedFrame&& frame) = 0;
    
    /// @brief 处理单帧（异步）
    using AlgorithmCallback = std::function<void(AlgorithmResult&& result)>;
    virtual bool ProcessAsync(DecodedFrame&& frame, 
                             AlgorithmCallback callback) = 0;
    
    /// @brief 获取算法名称
    virtual std::string GetName() const = 0;
    
    /// @brief 检查算法是否可用
    virtual bool IsAvailable() const = 0;
};
```

**设计要点**：
- ✅ 组合模式：内部使用 Preprocessor + InferenceEngine + Postprocessor
- ✅ 同步和异步处理
- ✅ 移动语义传递帧数据

---

## 🔄 数据流设计

### CPU 路径（零拷贝优化）

```
┌──────────────┐
│  Puller      │ NaluView (视图，零拷贝)
└──────┬───────┘
       │
       ▼
┌──────────────┐
│  Decoder     │ DecodedFrame (cv::Mat, 1次拷贝)
└──────┬───────┘
       │
       ▼
┌──────────────┐
│ Preprocessor │ TensorData (CPU, 1次拷贝)
└──────┬───────┘
       │
       ▼
┌──────────────┐
│ Inference    │ InferenceOutput (CPU, 0次拷贝)
└──────┬───────┘
       │
       ▼
┌──────────────┐
│Postprocessor │ DetectionResult (<1KB, 0次拷贝)
└──────────────┘

总拷贝次数: 2 次
```

### GPU 路径（近零拷贝）

```
┌──────────────┐
│  Puller      │ NaluView → GPU DMA (1次拷贝)
└──────┬───────┘
       │
       ▼
┌──────────────┐
│  Decoder     │ DecodedFrame (GPU, 0次拷贝)
└──────┬───────┘
       │
       ▼
┌──────────────┐
│ Preprocessor │ TensorData (GPU, 0次拷贝)
└──────┬───────┘
       │
       ▼
┌──────────────┐
│ Inference    │ InferenceOutput (GPU, 0次拷贝)
└──────┬───────┘
       │
       ▼ (DMA 传回 CPU)
┌──────────────┐
│Postprocessor │ DetectionResult (<1KB, 1次拷贝)
└──────────────┘

总拷贝次数: 2 次 (DMA 传输)
```

---

## 🛠️ 工厂模式实现

### ProcessorFactory

**文件**: `modules/alg/include/alg/processor_factory.h`

```cpp
#pragma once

#include <memory>
#include <string>
#include <map>

// 前向声明
class IPuller;
class IDecoder;
class IPreprocessor;
class IInferenceEngine;
class IPostprocessor;
class IAlgorithm;

struct PullerConfig;
struct DecoderConfig;
struct PreprocessConfig;
struct InferenceConfig;
struct PostprocessConfig;
struct AlgorithmConfig;

/// @brief 处理器工厂
class ProcessorFactory {
public:
    /// @brief 创建拉流器
    static std::unique_ptr<IPuller> CreatePuller(const std::string& type,
                                                 const PullerConfig& config);
    
    /// @brief 创建解码器
    static std::unique_ptr<IDecoder> CreateDecoder(
        const std::string& type, const DecoderConfig& config);
    
    /// @brief 创建预处理器
    static std::unique_ptr<IPreprocessor> CreatePreprocessor(
        const std::string& type, const PreprocessConfig& config);
    
    /// @brief 创建推理引擎
    static std::unique_ptr<IInferenceEngine> CreateInferenceEngine(
        const std::string& type, const InferenceConfig& config);
    
    /// @brief 创建后处理器
    static std::unique_ptr<IPostprocessor> CreatePostprocessor(
        const std::string& type, const PostprocessConfig& config);
    
    /// @brief 创建算法
    static std::unique_ptr<IAlgorithm> CreateAlgorithm(
        const std::string& type, const AlgorithmConfig& config);
    
    /// @brief 注册自定义处理器
    using CreatorFunc = std::function<std::unique_ptr<void>()>;
    static void RegisterCreator(const std::string& name, CreatorFunc creator);
    
private:
    static std::map<std::string, CreatorFunc>& GetCreators();
};
```

**使用示例**：

```cpp
// 创建 GPU 加速流水线
DecoderConfig decoder_cfg;
decoder_cfg.type = DecoderType::NVDEC_GPU;
decoder_cfg.gpu_device_id = 0;
auto decoder = ProcessorFactory::CreateDecoder("nvdec", decoder_cfg);

PreprocessConfig preprocess_cfg;
preprocess_cfg.device = PreprocessDevice::CUDA;
auto preprocessor = ProcessorFactory::CreatePreprocessor("cuda", preprocess_cfg);

InferenceConfig infer_cfg;
infer_cfg.type = InferenceEngineType::TENSORRT;
infer_cfg.model_path = "yolov5.engine";
auto engine = ProcessorFactory::CreateInferenceEngine("tensorrt", infer_cfg);
```

---

## 💡 零拷贝优化技术

### 1. 内存池管理

```cpp
template<typename T>
class MemoryPool {
private:
    struct Block {
        alignas(64) char data[sizeof(T)];
        std::atomic<bool> in_use{false};
    };
    
    std::vector<Block> blocks_;
    
public:
    explicit MemoryPool(size_t capacity) : blocks_(capacity) {}
    
    T* Allocate() {
        for (auto& block : blocks_) {
            bool expected = false;
            if (block.in_use.compare_exchange_strong(expected, true)) {
                return reinterpret_cast<T*>(block.data);
            }
        }
        return nullptr;
    }
    
    void Release(T* ptr) {
        auto* block = reinterpret_cast<Block*>(
            reinterpret_cast<char*>(ptr) - offsetof(Block, data));
        block->in_use.store(false);
    }
};

// 使用
MemoryPool<DecodedFrame> frame_pool_(10);
auto* frame = frame_pool_.Allocate();
// ... 使用 frame
frame_pool_.Release(frame);
```

### 2. GPU 缓冲区池

```cpp
class GpuBufferPool {
private:
    std::vector<CUdeviceptr> y_buffers_;
    std::vector<CUdeviceptr> uv_buffers_;
    std::queue<int> available_indices_;
    std::mutex mutex_;
    
public:
    void Initialize(int width, int height, int pool_size = 4) {
        int y_size = width * height;
        int uv_size = y_size / 2;
        
        for (int i = 0; i < pool_size; ++i) {
            CUdeviceptr d_y, d_uv;
            cuMemAlloc(&d_y, y_size);
            cuMemAlloc(&d_uv, uv_size);
            y_buffers_.push_back(d_y);
            uv_buffers_.push_back(d_uv);
            available_indices_.push(i);
        }
    }
    
    std::pair<CUdeviceptr, CUdeviceptr> Acquire() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (available_indices_.empty()) {
            return {0, 0};
        }
        int idx = available_indices_.front();
        available_indices_.pop();
        return {y_buffers_[idx], uv_buffers_[idx]};
    }
    
    void Release(CUdeviceptr y_ptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = std::find(y_buffers_.begin(), y_buffers_.end(), y_ptr);
        if (it != y_buffers_.end()) {
            available_indices_.push(std::distance(y_buffers_.begin(), it));
        }
    }
};
```

### 3. CUDA 异步流水线

```cpp
class CudaAsyncPipeline {
private:
    cudaStream_t streams_[4];
    GpuBufferPool buffer_pool_;
    
public:
    void Initialize() {
        for (int i = 0; i < 4; ++i) {
            cudaStreamCreate(&streams_[i]);
        }
        buffer_pool_.Initialize(1920, 1080, 4);
    }
    
    void ProcessFrameAsync(const uint8_t* h_y, const uint8_t* h_u, 
                          const uint8_t* h_v, int width, int height) {
        // 1. 获取空闲缓冲区
        auto [d_y, d_uv] = buffer_pool_.Acquire();
        
        // 2. 异步 DMA: CPU → GPU
        cudaStream_t stream = streams_[current_stream_idx_];
        current_stream_idx_ = (current_stream_idx_ + 1) % 4;
        
        cudaMemcpyAsync(d_y, h_y, width * height, 
                       cudaMemcpyHostToDevice, stream);
        cudaMemcpyAsync(d_uv, h_u, width * height / 2, 
                       cudaMemcpyHostToDevice, stream);
        
        // 3. GPU 处理
        LaunchKernel(d_y, d_uv, stream);
        
        // 4. 异步回调
        cudaStreamAddCallback(stream, OnComplete, this, 0);
    }
    
private:
    static void OnComplete(cudaStream_t stream, cudaError_t status, 
                          void* user_data) {
        auto* self = static_cast<CudaAsyncPipeline*>(user_data);
        // 释放缓冲区
        // 通知上层
    }
    
    int current_stream_idx_ = 0;
};
```

---

## 📊 性能对比

| 方案 | 拷贝次数 | 延迟 | 吞吐量 | CPU | GPU | 复杂度 |
|------|---------|------|--------|-----|-----|--------|
| **当前架构** | 11-18 | 50-100ms | 10-20 FPS | 60-80% | 0% | 中 |
| **C++ CPU** | 2-3 | 10-20ms | 50-80 FPS | 60-80% | 0% | 中 |
| **C++ GPU** | 1-2 | 2-5ms | 200+ FPS | <10% | 30-50% | 高 |
| **混合架构** | 1-2 | 5-10ms | 100-150 FPS | 20-30% | 40-60% | 中高 |

---

## 🎯 实施建议

### Phase 1: 基础接口（1-2周）
1. ✅ 定义所有核心接口
2. ✅ 实现工厂模式
3. ✅ 实现内存池
4. ✅ 编写单元测试

### Phase 2: CPU 实现（2-3周）
1. ✅ FFmpeg 解码器
2. ✅ CPU 预处理器
3. ✅ OpenVINO 推理引擎
4. ✅ 端到端测试

### Phase 3: GPU 实现（3-4周）
1. ✅ NVDEC 解码器
2. ✅ CUDA 预处理器
3. ✅ TensorRT 推理引擎
4. ✅ GPU 缓冲区池
5. ✅ 性能优化

### Phase 4: 集成和优化（1-2周）
1. ✅ 集成到 VideoPipeline
2. ✅ 配置系统
3. ✅ 性能调优
4. ✅ 文档完善

---

## 📚 参考资料

1. [NVIDIA Video Codec SDK](https://developer.nvidia.com/video-codec-sdk)
2. [TensorRT Developer Guide](https://docs.nvidia.com/deeplearning/tensorrt/)
3. [OpenVINO Documentation](https://docs.openvino.ai/)
4. [CUDA Best Practices](https://docs.nvidia.com/cuda/cuda-c-best-practices-guide/)

---

**文档版本**: v1.0  
**最后更新**: 2026-04-29  
**作者**: Lingma AI Assistant
