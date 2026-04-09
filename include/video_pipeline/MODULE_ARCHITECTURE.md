# Video Pipeline 模块架构说明

## 目录结构

```
include/video_pipeline/
├── format_converter/          # 格式转换模块
│   ├── i_format_converter.h   # 格式转换接口
│   └── opencv_format_converter.h  # OpenCV 格式转换实现
│
├── algorithm_processor/       # 算法处理模块
│   ├── i_algorithm_processor.h    # 算法处理器接口
│   └── grpc_algorithm_processor.h # gRPC 算法处理器实现
│
├── decoder/                   # 解码器模块
├── puller/                    # 拉流器模块
├── output/                    # 输出模块
└── processor/                 # [已废弃] 旧的 processor 目录
```

## 模块职责

### 1. Format Converter（格式转换）

**位置**: `include/video_pipeline/format_converter/`

**职责**: 
- 将 FFmpeg 解码的 `VideoFrame`（YUV/RGB 原始数据）转换为 `cv::Mat`（BGR 格式）
- 颜色空间转换
- 图像缩放、旋转等基础处理
- **同步处理**，在解码线程中执行

**核心接口**:
```cpp
namespace video_pipeline {
namespace format_converter {

class IFormatConverter {
    virtual cv::Mat process(cv::Mat&& input) = 0;
};

class OpenCVFormatConverter {
    void process(VideoFrame&& frame, ProcessedCallback cb);
};

} // namespace format_converter
} // namespace video_pipeline
```

**数据流**:
```
Decoder → VideoFrame (YUV) → OpenCVFormatConverter → cv::Mat (BGR)
```

---

### 2. Algorithm Processor（算法处理）

**位置**: `include/video_pipeline/algorithm_processor/`

**职责**:
- 智能算法推理（目标检测、跟踪、分析等）
- 支持多种后端（Python gRPC、原生 C++、TensorRT 等）
- **异步处理**，独立线程或远程服务
- 返回结构化结果（DetectionResult）

**核心接口**:
```cpp
namespace video_pipeline {
namespace algorithm_processor {

class IAlgorithmProcessor {
    virtual bool Start() = 0;
    virtual bool ProcessFrame(const VideoFrame& frame) = 0;
    virtual void SetDetectionCallback(DetectionCallback callback) = 0;
    virtual ProcessorStats GetStats() const = 0;
};

class GrpcAlgorithmProcessor : public IAlgorithmProcessor {
    // gRPC 后端实现
};

// 未来可扩展
class NativeCppAlgorithmProcessor : public IAlgorithmProcessor {
    // 原生 C++ 后端实现
};

} // namespace algorithm_processor
} // namespace video_pipeline
```

**数据流**:
```
Decoder → VideoFrame (JPEG) → IAlgorithmProcessor → DetectionResult
                              (AI 推理)
```

---

## 完整数据流

```
VideoPipeline
    │
    ├─→ Puller (ZLM/HTTP-FLV)
    │       ↓
    ├─→ Decoder (FFmpeg)
    │       ↓
    │   VideoFrame (YUV/RGB)
    │       ↓
    │   ┌──────────────┐
    │   ↓              │
    │   Format         │
    │   Converter      │
    │   (OpenCV)       │
    │   ↓              │
    │   cv::Mat (BGR)  │
    │   ↓              │
    │   Display/       │
    │   Storage        │
    │                  │
    │                  ├─→ Algorithm Processor
    │                  │   (gRPC/Native)
    │                  │   ↓
    │                  │   DetectionResult
    │                  │   ↓
    │                  │   OSD Renderer
    │                  │   ↓
    │                  └─→ Display
    └─────────────────────────────
```

---

## 命名规范

| 模块 | 命名空间 | 类名前缀 | 示例 |
|------|---------|---------|------|
| 格式转换 | `video_pipeline::format_converter` | - | `OpenCVFormatConverter` |
| 算法处理 | `video_pipeline::algorithm_processor` | - | `GrpcAlgorithmProcessor` |
| 解码器 | `video_pipeline::decoder` | - | `FFmpegDecoder` |
| 拉流器 | `video_pipeline::puller` | - | `ZLMPuller` |

---

## 迁移指南

### 从旧代码迁移

如果代码中使用了旧的 `IProcessor` 或 `OpenCVFrameProcessor`：

**旧代码**:
```cpp
#include "video_pipeline/processor/i_processor.h"
#include "video_pipeline/processor/opencv_processor.h"

class MyProcessor : public IProcessor { ... };
auto processor = std::make_unique<OpenCVFrameProcessor>();
```

**新代码**:
```cpp
#include "video_pipeline/format_converter/i_format_converter.h"
#include "video_pipeline/format_converter/opencv_format_converter.h"

namespace vpfc = video_pipeline::format_converter;

class MyConverter : public vpfc::IFormatConverter { ... };
auto converter = std::make_unique<vpfc::OpenCVFormatConverter>();
```

### 使用算法处理器

```cpp
#include "video_pipeline/algorithm_processor/i_algorithm_processor.h"
#include "video_pipeline/algorithm_processor/grpc_algorithm_processor.h"

namespace vpap = video_pipeline::algorithm_processor;

// 创建配置
vpap::ProcessorConfig config;
config.type = vpap::ProcessorType::GRPC_PYTHON;
config.grpc_address = "localhost:50052";
config.grpc_target_fps = 10;

// 创建处理器
auto processor = std::make_unique<vpap::GrpcAlgorithmProcessor>(config);

// 设置回调
processor->SetDetectionCallback([](const vpap::DetectionResult& result) {
    std::cout << "Detected " << result.boxes.size() << " objects" << std::endl;
});

// 启动
processor->Start();

// 处理帧
vpap::VideoFrame frame;
frame.data = jpeg_data;
frame.width = 640;
frame.height = 480;
processor->ProcessFrame(frame);

// 停止
processor->Stop();
```

---

## 扩展指南

### 添加新的格式转换器

```cpp
// include/video_pipeline/format_converter/custom_format_converter.h
namespace video_pipeline {
namespace format_converter {

class CustomFormatConverter : public IFormatConverter {
public:
    cv::Mat process(cv::Mat&& input) override {
        // 自定义格式转换逻辑
        return input;
    }
};

} // namespace format_converter
} // namespace video_pipeline
```

### 添加新的算法处理器

```cpp
// include/video_pipeline/algorithm_processor/native_algorithm_processor.h
namespace video_pipeline {
namespace algorithm_processor {

class NativeAlgorithmProcessor : public IAlgorithmProcessor {
public:
    bool Start() override;
    bool ProcessFrame(const VideoFrame& frame) override;
    void SetDetectionCallback(DetectionCallback callback) override;
    ProcessorStats GetStats() const override;
    bool IsAvailable() const override;
    ProcessorType GetType() const override { 
        return ProcessorType::NATIVE_CPP; 
    }

private:
    std::unique_ptr<YoloDetector> detector_;
};

} // namespace algorithm_processor
} // namespace video_pipeline
```

---

## 总结

✅ **清晰的职责分离**: 格式转换 vs 算法处理  
✅ **避免命名冲突**: 明确的模块命名  
✅ **易于扩展**: 插件式架构  
✅ **平滑迁移**: 保留旧代码兼容性  
✅ **面向未来**: 支持多种算法后端  

这个架构让你可以：
- 🎯 清楚区分格式转换和算法处理
- 🔧 轻松添加新的转换器或处理器
- 🚀 从 Python gRPC 无缝切换到 C++ 算法
- 📦 模块化设计，便于维护和测试
