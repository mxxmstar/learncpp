# PipelineConfig 模块化配置使用指南

## 🎯 概述

`PipelineConfig` 已经重构为模块化结构，将配置拆分为多个子配置结构，使代码更清晰、更易维护。

---

## 📊 配置结构

### 新的模块化结构

```
PipelineConfig
├── channel_id: int                    # 通道 ID
├── puller: PullerConfig              # 拉流配置
│   ├── stream_url: string
│   ├── reconnect_delay: int
│   ├── max_reconnect_attempts: int
│   └── pull_timeout_ms: int
├── decoder: DecoderConfig            # 解码配置
│   ├── decoder_threads: int
│   ├── output_format: int
│   ├── raw_queue_size: int
│   └── decoded_queue_size: int
├── preprocess: PreprocessConfig      # 预处理配置
│   ├── enable_preprocess: bool
│   ├── filters: vector<string>
│   ├── target_width: int
│   ├── target_height: int
│   └── processed_queue_size: int
├── algorithm: AlgorithmConfig        # 算法配置
│   ├── grpc: GrpcAlgorithmConfig    # gRPC 远程算法
│   │   ├── enabled: bool
│   │   ├── server_address: string
│   │   └── target_fps: int
│   ├── openvino: OpenVINOAlgorithmConfig  # OpenVINO 本地算法
│   │   ├── enabled: bool
│   │   ├── model_path: string
│   │   ├── device: string
│   │   ├── confidence_threshold: float
│   │   └── batch_size: int
│   └── opencv: OpenCVAlgorithmConfig # OpenCV 本地算法
│       ├── enabled: bool
│       ├── algorithm_type: string
│       ├── config_path: string
│       └── confidence_threshold: float
├── recording: RecordingConfig        # 录制配置
│   ├── save_raw_data: bool
│   └── save_path: string
└── log: LogConfig                    # 日志配置
    └── log_level: int
```

---

## 🚀 快速开始

### 示例 1: 基本用法（手动配置）

```cpp
#include "videopipeline/pipeline_config.h"

PipelineConfig config;

// 设置通道 ID
config.channel_id = 1;

// 配置拉流
config.puller.stream_url = "rtsp://192.168.1.100:554/stream";
config.puller.reconnect_delay = 5;
config.puller.max_reconnect_attempts = 10;
config.puller.pull_timeout_ms = 3000;

// 配置解码
config.decoder.decoder_threads = 4;
config.decoder.raw_queue_size = 128;
config.decoder.decoded_queue_size = 32;

// 配置预处理
config.preprocess.enable_preprocess = true;
config.preprocess.filters = {"resize", "gaussian_blur"};
config.preprocess.target_width = 640;
config.preprocess.target_height = 480;

// 配置 gRPC 算法
config.algorithm.grpc.enabled = true;
config.algorithm.grpc.server_address = "localhost:50053";
config.algorithm.grpc.target_fps = 15;

// 配置录制
config.recording.save_raw_data = false;
config.recording.save_path = "./recordings";

// 配置日志
config.log.log_level = 1;  // INFO

// 验证配置
if (config.isValid()) {
    auto pipeline = VideoPipelineManager::getInstance().createPipeline(config);
}
```

---

### 示例 2: 使用便捷方法创建 gRPC 配置

```cpp
// ✅ 简洁方式：一行代码创建 gRPC 配置
auto config = PipelineConfig::createWithGrpc(
    "rtsp://192.168.1.100:554/stream",
    "localhost:50053",
    15  // FPS
);

// 可以进一步自定义
config.decoder.decoder_threads = 4;
config.preprocess.filters = {"resize"};
config.preprocess.target_width = 640;
config.preprocess.target_height = 480;
```

---

### 示例 3: 使用便捷方法创建 OpenVINO 配置

```cpp
// ✅ 简洁方式：一行代码创建 OpenVINO 配置
auto config = PipelineConfig::createWithOpenVINO(
    "rtsp://192.168.1.100:554/stream",
    "models/yolov5s.xml",
    "CPU",           // 设备类型
    0.6f             // 置信度阈值
);

// 可以进一步自定义
config.algorithm.openvino.batch_size = 2;
config.preprocess.enable_preprocess = false;  // OpenVINO 内部处理
```

---

### 示例 4: 使用便捷方法创建 OpenCV 配置

```cpp
// ✅ 简洁方式：一行代码创建 OpenCV 配置
auto config = PipelineConfig::createWithOpenCV(
    "rtsp://192.168.1.100:554/stream",
    "face_detect",              // 算法类型
    "models/haarcascade_frontalface.xml",  // 配置文件
    0.7f                         // 置信度阈值
);

// 可以进一步自定义
config.algorithm.opencv.confidence_threshold = 0.8f;
```

---

## 💡 高级用法

### 场景 1: 多路视频流，不同算法后端

```cpp
class MultiStreamManager {
public:
    void addStreams() {
        // 流 1: gRPC 远程算法
        auto config1 = PipelineConfig::createWithGrpc(
            "rtsp://camera1/stream",
            "grpc-server:50053",
            10
        );
        config1.channel_id = 1;
        manager_.addStream(config1);
        
        // 流 2: OpenVINO 本地推理
        auto config2 = PipelineConfig::createWithOpenVINO(
            "rtsp://camera2/stream",
            "models/yolov8.xml",
            "GPU"  // 使用 GPU 加速
        );
        config2.channel_id = 2;
        manager_.addStream(config2);
        
        // 流 3: OpenCV 人脸检测
        auto config3 = PipelineConfig::createWithOpenCV(
            "rtsp://camera3/stream",
            "face_detect",
            "models/haarcascade.xml"
        );
        config3.channel_id = 3;
        manager_.addStream(config3);
    }
    
private:
    VideoPipelineManager manager_;
};
```

---

### 场景 2: 动态切换算法后端

```cpp
class AdaptivePipeline {
private:
    std::string stream_url_ = "rtsp://camera/stream";
    
public:
    void switchToGrpc() {
        auto config = PipelineConfig::createWithGrpc(
            stream_url_,
            "grpc-server:50053",
            15
        );
        recreatePipeline(config);
    }
    
    void switchToOpenVINO() {
        auto config = PipelineConfig::createWithOpenVINO(
            stream_url_,
            "models/yolov5s.xml",
            "CPU"
        );
        recreatePipeline(config);
    }
    
    void switchToOpenCV() {
        auto config = PipelineConfig::createWithOpenCV(
            stream_url_,
            "motion_detect"
        );
        recreatePipeline(config);
    }
    
private:
    void recreatePipeline(const PipelineConfig& config) {
        // 销毁旧流水线
        // 创建新流水线
        // ...
    }
};
```

---

### 场景 3: 根据负载自动选择算法

```cpp
class LoadBalancedPipeline {
public:
    PipelineConfig selectAlgorithm(float cpu_usage, float gpu_usage) {
        if (cpu_usage < 30.0f && gpu_usage < 50.0f) {
            // 低负载：使用 OpenVINO GPU（最快）
            return PipelineConfig::createWithOpenVINO(
                stream_url_,
                "models/yolov5s.xml",
                "GPU"
            );
        } else if (cpu_usage < 60.0f) {
            // 中等负载：使用 OpenVINO CPU
            return PipelineConfig::createWithOpenVINO(
                stream_url_,
                "models/yolov5s.xml",
                "CPU"
            );
        } else {
            // 高负载：使用 gRPC 远程算法
            return PipelineConfig::createWithGrpc(
                stream_url_,
                "remote-server:50053",
                10
            );
        }
    }
    
private:
    std::string stream_url_;
};
```

---

### 场景 4: 批量创建相同配置的流水线

```cpp
class BatchPipelineCreator {
public:
    std::vector<PipelineConfig> createBatch(
        const std::vector<std::string>& stream_urls,
        const std::string& algorithm_type = "grpc") {
        
        std::vector<PipelineConfig> configs;
        
        for (size_t i = 0; i < stream_urls.size(); i++) {
            PipelineConfig config;
            config.channel_id = static_cast<int>(i + 1);
            config.puller.stream_url = stream_urls[i];
            
            // 统一配置
            config.decoder.decoder_threads = 2;
            config.decoder.raw_queue_size = 64;
            config.preprocess.enable_preprocess = true;
            config.preprocess.filters = {"resize"};
            config.preprocess.target_width = 640;
            config.preprocess.target_height = 480;
            
            // 根据类型配置算法
            if (algorithm_type == "grpc") {
                config.algorithm.grpc.enabled = true;
                config.algorithm.grpc.server_address = "grpc-server:50053";
                config.algorithm.grpc.target_fps = 10;
            } else if (algorithm_type == "openvino") {
                config.algorithm.openvino.enabled = true;
                config.algorithm.openvino.model_path = "models/yolov5s.xml";
                config.algorithm.openvino.device = "CPU";
            }
            
            configs.push_back(config);
        }
        
        return configs;
    }
};

// 使用
std::vector<std::string> urls = {
    "rtsp://camera1/stream",
    "rtsp://camera2/stream",
    "rtsp://camera3/stream",
    "rtsp://camera4/stream"
};

auto creator = BatchPipelineCreator();
auto configs = creator.createBatch(urls, "openvino");

for (auto& config : configs) {
    manager.addStream(config);
}
```

---

## 🔧 配置验证

### 检查配置有效性

```cpp
PipelineConfig config;

// 基本验证
if (!config.isValid()) {
    LOG_MAIN_ERROR_AT("Invalid config: stream_url is empty");
    return;
}

// 检查启用的算法
std::string algo = config.algorithm.getActiveAlgorithm();
LOG_MAIN_INFO_AT("Active algorithm: {}", algo);

if (algo == "none") {
    LOG_MAIN_WARN_AT("No algorithm enabled");
}

// 检查具体算法配置
if (config.algorithm.grpc.enabled) {
    if (!config.algorithm.grpc.isValid()) {
        LOG_MAIN_ERROR_AT("Invalid gRPC config");
    }
}

if (config.algorithm.openvino.enabled) {
    if (!config.algorithm.openvino.isValid()) {
        LOG_MAIN_ERROR_AT("Invalid OpenVINO config: model_path is empty");
    }
}

if (config.algorithm.opencv.enabled) {
    if (!config.algorithm.opencv.isValid()) {
        LOG_MAIN_ERROR_AT("Invalid OpenCV config: algorithm_type is none");
    }
}
```

---

## 📈 迁移指南

### 从旧配置迁移到新配置

#### 旧代码

```cpp
// ❌ 旧方式（扁平结构）
PipelineConfig old_config;
old_config.stream_url = "rtsp://...";
old_config.decoder_threads = 2;
old_config.enable_preprocess = true;
old_config.algorithm_type = "yolo_v5";
old_config.model_path = "models/yolov5.xml";
old_config.enable_grpc_send = true;
old_config.grpc_server_address = "localhost:50053";
old_config.grpc_target_fps = 10;
```

#### 新代码

```cpp
// ✅ 新方式（模块化结构）
PipelineConfig new_config;

// 拉流配置
new_config.puller.stream_url = "rtsp://...";

// 解码配置
new_config.decoder.decoder_threads = 2;

// 预处理配置
new_config.preprocess.enable_preprocess = true;

// 算法配置（选择一种）
// 选项 1: gRPC
new_config.algorithm.grpc.enabled = true;
new_config.algorithm.grpc.server_address = "localhost:50053";
new_config.algorithm.grpc.target_fps = 10;

// 选项 2: OpenVINO
// new_config.algorithm.openvino.enabled = true;
// new_config.algorithm.openvino.model_path = "models/yolov5.xml";
// new_config.algorithm.openvino.device = "CPU";

// 或者使用便捷方法
auto config = PipelineConfig::createWithGrpc(
    "rtsp://...",
    "localhost:50053",
    10
);
```

---

## 🎓 最佳实践

### 1. 使用便捷方法创建配置

```cpp
// ✅ 推荐：使用便捷方法
auto config = PipelineConfig::createWithOpenVINO(url, model, device);

// ❌ 不推荐：手动设置所有字段
PipelineConfig config;
config.puller.stream_url = url;
config.algorithm.openvino.enabled = true;
config.algorithm.openvino.model_path = model;
// ... 需要设置很多字段
```

### 2. 集中管理常用配置

```cpp
class ConfigTemplates {
public:
    static PipelineConfig highPerformanceGrpc(const std::string& url) {
        auto config = PipelineConfig::createWithGrpc(url, "grpc-server:50053", 30);
        config.decoder.decoder_threads = 4;
        config.decoder.raw_queue_size = 128;
        config.preprocess.enable_preprocess = false;
        return config;
    }
    
    static PipelineConfig lowLatencyOpenVINO(const std::string& url) {
        auto config = PipelineConfig::createWithOpenVINO(url, "models/yolov5s.xml", "GPU");
        config.decoder.decoder_threads = 2;
        config.algorithm.openvino.batch_size = 1;
        config.preprocess.enable_preprocess = false;
        return config;
    }
    
    static PipelineConfig balancedOpenCV(const std::string& url) {
        auto config = PipelineConfig::createWithOpenCV(url, "face_detect");
        config.decoder.decoder_threads = 2;
        config.preprocess.enable_preprocess = true;
        config.preprocess.filters = {"resize"};
        config.preprocess.target_width = 640;
        return config;
    }
};

// 使用
auto config = ConfigTemplates::highPerformanceGrpc("rtsp://camera/stream");
```

### 3. 配置序列化（保存到文件）

```cpp
#include <nlohmann/json.hpp>

// 保存配置到 JSON
void saveConfig(const PipelineConfig& config, const std::string& filename) {
    nlohmann::json j;
    
    j["channel_id"] = config.channel_id;
    
    j["puller"]["stream_url"] = config.puller.stream_url;
    j["puller"]["reconnect_delay"] = config.puller.reconnect_delay;
    
    j["decoder"]["decoder_threads"] = config.decoder.decoder_threads;
    j["decoder"]["raw_queue_size"] = config.decoder.raw_queue_size;
    
    j["algorithm"]["grpc"]["enabled"] = config.algorithm.grpc.enabled;
    j["algorithm"]["grpc"]["server_address"] = config.algorithm.grpc.server_address;
    
    j["algorithm"]["openvino"]["enabled"] = config.algorithm.openvino.enabled;
    j["algorithm"]["openvino"]["model_path"] = config.algorithm.openvino.model_path;
    
    // ... 其他字段
    
    std::ofstream file(filename);
    file << j.dump(2);
}

// 从 JSON 加载配置
PipelineConfig loadConfig(const std::string& filename) {
    std::ifstream file(filename);
    nlohmann::json j;
    file >> j;
    
    PipelineConfig config;
    config.channel_id = j.value("channel_id", 0);
    
    config.puller.stream_url = j["puller"].value("stream_url", "");
    config.puller.reconnect_delay = j["puller"].value("reconnect_delay", 3);
    
    config.decoder.decoder_threads = j["decoder"].value("decoder_threads", 2);
    
    config.algorithm.grpc.enabled = j["algorithm"]["grpc"].value("enabled", false);
    config.algorithm.grpc.server_address = j["algorithm"]["grpc"].value("server_address", "");
    
    // ... 其他字段
    
    return config;
}
```

---

## 🐛 常见问题

### Q1: 为什么要把配置拆分？

**A**: 
- ✅ **清晰的职责划分**：每个子配置负责一个模块
- ✅ **易于维护**：修改某个模块的配置不影响其他模块
- ✅ **类型安全**：编译器可以检查配置字段的正确性
- ✅ **可复用**：子配置可以独立使用

### Q2: 如何同时启用多个算法？

**A**: 目前设计为**互斥**的，一次只能启用一个算法后端：

```cpp
// ✅ 正确：只启用一个
config.algorithm.grpc.enabled = true;
config.algorithm.openvino.enabled = false;
config.algorithm.opencv.enabled = false;

// ❌ 不推荐：同时启用多个（行为未定义）
config.algorithm.grpc.enabled = true;
config.algorithm.openvino.enabled = true;  // 可能导致冲突
```

如果需要多个算法，可以创建多个 `VideoPipeline` 实例。

### Q3: 如何获取当前启用的算法？

**A**: 使用 `getActiveAlgorithm()` 方法：

```cpp
std::string algo = config.algorithm.getActiveAlgorithm();
// 返回: "grpc", "openvino", "opencv", 或 "none"

if (algo == "grpc") {
    // gRPC 算法逻辑
} else if (algo == "openvino") {
    // OpenVINO 算法逻辑
}
```

### Q4: 旧代码还能用吗？

**A**: 不能直接使用，需要迁移到新结构。但迁移很简单：

```cpp
// 旧代码
old_config.stream_url = "...";

// 新代码
new_config.puller.stream_url = "...";
```

只需在字段前加上对应的子配置名称即可。

---

## 📚 相关文档

- [pipeline_config.h](../include/videopipeline/pipeline_config.h) - 配置头文件
- [VideoPipeline 使用指南](./VIDEO_PIPELINE_USAGE.md) - 流水线使用文档

---

**更新日期**: 2026-05-04  
**作者**: Lingma AI Assistant  
**版本**: v2.0 (模块化配置)
