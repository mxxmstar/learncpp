# 视频处理器抽象层架构

## 概述

本架构实现了视频处理的抽象层，支持运行时切换不同的处理后端（Python gRPC、原生 C++ 等）。

## 设计目标

1. **解耦**：VideoPipeline 不依赖具体的处理实现
2. **可扩展**：轻松添加新的处理后端
3. **可测试**：每个组件可以独立测试
4. **平滑迁移**：从 Python gRPC 无缝切换到 C++ 算法

## 架构图

```
┌─────────────────────────────────────────────┐
│           VideoPipeline                     │
│                                             │
│  ┌──────────┐    ┌──────────────────┐      │
│  │ Decoder  │───►│ Frame Queue      │      │
│  └──────────┘    └────────┬─────────┘      │
│                           │                 │
│                           ▼                 │
│                  ┌────────────────┐         │
│                  │ IVideoProcessor│ (接口)  │
│                  │  (抽象层)      │         │
│                  └────────┬───────┘         │
│                           │                 │
│              ┌────────────┴────────────┐    │
│              │                         │    │
│     ┌────────▼────────┐      ┌────────▼──────┐
│     │GrpcVideoProcessor│      │NativeCppProcessor│
│     │  (当前使用)      │      │  (未来实现)   │
│     └────────┬────────┘      └────────┬──────┘
│              │                         │
└──────────────┼─────────────────────────┼────┘
               │                         │
               ▼                         ▼
      ┌────────────────┐       ┌────────────────┐
      │ Python Server  │       │ C++ Algorithm  │
      │  (YOLOv5)      │       │  (ONNX/OpenVINO)│
      └────────────────┘       └────────────────┘
```

## 核心组件

### 1. IVideoProcessor（抽象接口）

**位置**: `include/video_processor.h`

**职责**:
- 定义统一的视频处理接口
- 声明所有处理器必须实现的方法

**关键方法**:
```cpp
class IVideoProcessor {
public:
    virtual bool Start() = 0;
    virtual void Stop() = 0;
    virtual bool ProcessFrame(const VideoFrame& frame) = 0;
    virtual void SetDetectionCallback(DetectionCallback callback) = 0;
    virtual ProcessorStats GetStats() const = 0;
    virtual bool IsAvailable() const = 0;
    virtual ProcessorType GetType() const = 0;
};
```

### 2. GrpcVideoProcessor（gRPC 实现）

**位置**: 
- 头文件: `include/grpc_video_processor.h`
- 实现: `src/grpc_video_processor.cpp`

**职责**:
- 适配 VideoGrpcClient 到 IVideoProcessor 接口
- 管理 gRPC 连接和通信
- 实现帧率控制
- 转换检测结果格式

**特性**:
- ✅ 自动帧率控制（可配置）
- ✅ 统计信息收集
- ✅ 错误处理和重连
- ✅ 线程安全

### 3. VideoFrame & DetectionResult（数据结构）

**位置**: `include/video_processor.h`

**VideoFrame**:
```cpp
struct VideoFrame {
    std::vector<uint8_t> data;  // JPEG 编码
    int width;
    int height;
    std::string frame_id;
    int64_t timestamp;
};
```

**DetectionResult**:
```cpp
struct DetectionResult {
    std::string frame_id;
    std::vector<BoundingBox> boxes;
    int64_t processing_time_ms;
    std::string algorithm;
    std::map<std::string, std::string> metadata;
};
```

## 使用示例

### 基本用法

```cpp
#include "video_processor.h"
#include "grpc_video_processor.h"

using namespace video_processor;

// 1. 创建配置
ProcessorConfig config;
config.type = ProcessorType::GRPC_PYTHON;
config.grpc_address = "localhost:50052";
config.grpc_target_fps = 10;

// 2. 创建处理器
auto processor = std::make_unique<GrpcVideoProcessor>(config);

// 3. 设置回调
processor->SetDetectionCallback([](const DetectionResult& result) {
    std::cout << "Detected " << result.boxes.size() << " objects" << std::endl;
});

// 4. 启动
if (!processor->Start()) {
    std::cerr << "Failed to start" << std::endl;
    return;
}

// 5. 处理帧
VideoFrame frame;
frame.data = jpeg_data;
frame.width = 640;
frame.height = 480;
frame.frame_id = "frame_001";

processor->ProcessFrame(frame);

// 6. 停止
processor->Stop();
```

### 在 VideoPipeline 中使用（未来）

```cpp
class VideoPipeline {
private:
    std::unique_ptr<IVideoProcessor> processor_;
    
public:
    bool EnableVideoProcessing(const ProcessorConfig& config) {
        // 根据类型创建处理器
        switch (config.type) {
            case ProcessorType::GRPC_PYTHON:
                processor_ = std::make_unique<GrpcVideoProcessor>(config);
                break;
            case ProcessorType::NATIVE_CPP:
                // processor_ = std::make_unique<NativeCppProcessor>(config);
                break;
        }
        
        // 设置回调
        processor_->SetDetectionCallback([this](const DetectionResult& result) {
            HandleDetectionResult(result);
        });
        
        return processor_->Start();
    }
    
    void ProcessDecodedFrame(const cv::Mat& frame) {
        if (processor_ && processor_->IsAvailable()) {
            // 编码为 JPEG
            std::vector<uchar> buf;
            cv::imencode(".jpg", frame, buf);
            
            VideoFrame video_frame;
            video_frame.data.assign(buf.begin(), buf.end());
            video_frame.width = frame.cols;
            video_frame.height = frame.rows;
            
            processor_->ProcessFrame(video_frame);
        }
    }
};
```

## 扩展指南

### 添加新的处理器（例如 NativeCppProcessor）

#### Step 1: 创建类

```cpp
// include/native_cpp_processor.h
class NativeCppProcessor : public IVideoProcessor {
public:
    explicit NativeCppProcessor(const ProcessorConfig& config);
    ~NativeCppProcessor() override;
    
    bool Start() override;
    void Stop() override;
    bool ProcessFrame(const VideoFrame& frame) override;
    void SetDetectionCallback(DetectionCallback callback) override;
    ProcessorStats GetStats() const override;
    bool IsAvailable() const override;
    ProcessorType GetType() const override { 
        return ProcessorType::NATIVE_CPP; 
    }

private:
    std::unique_ptr<YoloDetector> detector_;
    // ... 其他成员
};
```

#### Step 2: 实现

```cpp
// src/native_cpp_processor.cpp
bool NativeCppProcessor::Start() {
    detector_ = std::make_unique<YoloDetector>(config_.model_path);
    return detector_->Initialize();
}

bool NativeCppProcessor::ProcessFrame(const VideoFrame& frame) {
    // 解码 JPEG
    cv::Mat image = cv::imdecode(frame.data, cv::IMREAD_COLOR);
    
    // 运行推理
    auto results = detector_->Detect(image);
    
    // 转换为 DetectionResult
    DetectionResult result = ConvertToDetectionResult(results);
    
    // 触发回调
    if (callback_) {
        callback_(result);
    }
    
    return true;
}
```

#### Step 3: 注册

在 VideoPipeline 或工厂函数中添加：

```cpp
case ProcessorType::NATIVE_CPP:
    processor_ = std::make_unique<NativeCppProcessor>(config);
    break;
```

## 配置示例

### YAML 配置

```yaml
video_processing:
  # 处理器类型
  processor_type: "grpc_python"  # 或 "native_cpp"
  
  # gRPC 配置
  grpc:
    server_address: "localhost:50052"
    target_fps: 10
  
  # 原生 C++ 配置
  native_cpp:
    model_path: "models/yolov5s.onnx"
    device: "cuda"  # 或 "cpu"
    confidence_threshold: 0.5
```

### 加载配置

```cpp
ProcessorConfig LoadConfig(const YAML::Node& config) {
    ProcessorConfig proc_config;
    
    auto type_str = config["processor_type"].as<std::string>();
    if (type_str == "grpc_python") {
        proc_config.type = ProcessorType::GRPC_PYTHON;
    } else if (type_str == "native_cpp") {
        proc_config.type = ProcessorType::NATIVE_CPP;
    }
    
    proc_config.grpc_address = config["grpc"]["server_address"].as<std::string>();
    proc_config.grpc_target_fps = config["grpc"]["target_fps"].as<int>();
    
    proc_config.model_path = config["native_cpp"]["model_path"].as<std::string>();
    proc_config.device = config["native_cpp"]["device"].as<std::string>();
    
    return proc_config;
}
```

## 性能考虑

### 1. 帧率控制

GrpcVideoProcessor 内置帧率控制器：

```cpp
bool ShouldSendFrame() {
    auto elapsed = now - last_frame_time_;
    int target_interval = 1000 / target_fps;
    
    if (elapsed < target_interval) {
        return false; // 跳过
    }
    
    last_frame_time_ = now;
    return true;
}
```

### 2. 异步处理

- gRPC 通信在独立线程中进行
- 不会阻塞解码线程
- 队列满时丢弃旧帧（保证实时性）

### 3. 统计信息

实时监控性能：

```cpp
auto stats = processor->GetStats();
std::cout << "FPS: " << stats.fps << std::endl;
std::cout << "Avg latency: " << stats.avg_processing_time_ms << "ms" << std::endl;
std::cout << "Failed frames: " << stats.frames_failed << std::endl;
```

## 迁移路径

### 阶段 1: 当前（Python gRPC）

```cpp
auto processor = std::make_unique<GrpcVideoProcessor>(config);
pipeline.SetProcessor(std::move(processor));
```

### 阶段 2: 并行开发 C++ 处理器

```cpp
// 团队 A: 维护 GrpcVideoProcessor
// 团队 B: 开发 NativeCppProcessor

// 可以 A/B 测试
if (use_native) {
    processor = std::make_unique<NativeCppProcessor>(config);
} else {
    processor = std::make_unique<GrpcVideoProcessor>(config);
}
```

### 阶段 3: 切换到 C++

```cpp
// 只需修改配置
config.type = ProcessorType::NATIVE_CPP;
config.model_path = "yolov5s.onnx";

// 代码无需修改
auto processor = CreateProcessor(config); // 工厂函数
```

### 阶段 4: 清理（可选）

如果不再需要 gRPC：
- 删除 GrpcVideoProcessor
- 保留 IVideoProcessor 接口（便于未来扩展）

## 测试

### 编译测试

```bash
cd out/build/x64-Debug
cmake --build . --target test_video_processor
```

### 运行测试

```bash
# 先启动 Python 服务器
cd algorithm/grpc_server
python video_service.py --port 50052

# 运行测试
cd out/build/x64-Debug
./bin/test_video_processor.exe
```

## 总结

✅ **接口抽象**: 完全解耦 VideoPipeline 和处理实现  
✅ **插件式架构**: 轻松替换后端  
✅ **平滑迁移**: 从 Python 到 C++ 无需重构  
✅ **生产就绪**: 完善的错误处理和统计  
✅ **易于测试**: 每个组件可独立测试  

这个架构让你可以：
- 🚀 现在快速使用 Python 原型
- 🔄 未来无缝切换到 C++
- 🎯 甚至同时支持多种后端
