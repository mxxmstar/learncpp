# Algorithm 模块

## 📋 概述

Algorithm 模块提供完整的 AI 算法处理能力，包括预处理、推理、后处理等完整流水线。

## 🏗️ 模块结构

```
modules/alg/
│
├── 📂 inference/          # ✅ 已完成 - 推理引擎模块
│   ├── OpenVINO CPU 引擎
│   ├── 异步推理支持
│   ├── 工厂模式扩展
│   └── 详细文档和示例
│
├── 📂 preprocess/         # ⏳ 待实现 - 预处理模块
│   ├── 图像缩放、归一化
│   ├── YUV → RGB 转换
│   └── CPU/GPU 实现
│
├── 📂 postprocess/        # ⏳ 待实现 - 后处理模块
│   ├── NMS（非极大值抑制）
│   ├── 检测框解析
│   └── 坐标还原
│
├── 📂 algorithms/         # ⏳ 待实现 - 具体算法实现
│   ├── YOLOv5
│   ├── YOLOv8
│   └── 自定义算法
│
└── 📂 grpc/               # 已存在 - gRPC 通信层
    └── 视频流传输
```

---

## ✅ 已完成模块

### Inference 模块

**状态**: ✅ 生产就绪  
**位置**: `modules/alg/inference/`

#### 功能特性

- ✅ **统一接口**: `IInferenceEngine` 抽象接口
- ✅ **多引擎支持**: OpenVINO、TensorRT（计划中）、ONNX Runtime（计划中）
- ✅ **同步/异步推理**: 灵活选择推理模式
- ✅ **批量推理**: 支持批量输入
- ✅ **零拷贝设计**: 最小化内存拷贝
- ✅ **并发安全**: 线程安全的统计和任务队列
- ✅ **工厂模式**: 易于扩展新引擎

#### 快速开始

```cpp
#include "alg/inference/inference_engine_factory.h"

// 创建引擎
auto engine = InferenceEngineFactory::Create("openvino_cpu", config);

// 执行推理
auto result = engine->Infer(input_tensor);

// 处理结果
if (result.success) {
    processOutput(result.tensors);
}
```

#### 文档

- 📘 [完整文档](inference/README.md)
- 🚀 [快速开始](inference/QUICKSTART.md)
- 📊 [实现总结](inference/IMPLEMENTATION_SUMMARY.md)
- 📁 [项目结构](inference/PROJECT_STRUCTURE.md)

#### 编译和测试

```bash
# Windows
cd modules/alg/inference
build_and_test.bat

# Linux/Mac
cd modules/alg/inference
chmod +x build_and_test.sh
./build_and_test.sh
```

---

## ⏳ 计划中的模块

### Preprocess 模块

**预计完成**: 2026-05-10

#### 计划功能

- [ ] `IPreprocessor` 接口定义
- [ ] CPU 预处理器（OpenCV）
  - [ ] 图像缩放（保持宽高比）
  - [ ] 归一化（mean/std）
  - [ ] 颜色空间转换（YUV→RGB, BGR→RGB）
  - [ ] HWC → CHW 格式转换
- [ ] GPU 预处理器（CUDA）
  - [ ] CUDA kernel 实现
  - [ ] 异步处理支持
- [ ] 元数据记录（用于后处理坐标还原）

#### 接口预览

```cpp
class IPreprocessor {
public:
    virtual bool Process(const cv::Mat& input, 
                        TensorData& output,
                        PreprocessMetadata& metadata) = 0;
    
    virtual bool ProcessGpu(void* gpu_yuv_frame,
                           TensorData& output,
                           PreprocessMetadata& metadata) = 0;
};
```

---

### Postprocess 模块

**预计完成**: 2026-05-17

#### 计划功能

- [ ] `IPostprocessor` 接口定义
- [ ] YOLO 系列后处理
  - [ ] 置信度过滤
  - [ ] NMS（非极大值抑制）
  - [ ] 坐标还原到原始图像
- [ ] 通用检测框解析
- [ ] 分类结果解析
- [ ] 分割掩码处理（未来）

#### 接口预览

```cpp
class IPostprocessor {
public:
    virtual DetectionResult Process(
        const TensorData& raw_output,
        const PreprocessMetadata& metadata) = 0;
};
```

---

### Algorithms 模块

**预计完成**: 2026-05-24

#### 计划功能

- [ ] `IAlgorithm` 接口定义
- [ ] YOLOv5 实现
- [ ] YOLOv8 实现
- [ ] 组合 Preprocessor + Inference + Postprocessor
- [ ] 简化的 API
- [ ] 算法注册机制

#### 接口预览

```cpp
class IAlgorithm {
public:
    virtual AlgorithmResult Process(DecodedFrame&& frame) = 0;
    
    virtual bool ProcessAsync(DecodedFrame&& frame,
                             AlgorithmCallback callback) = 0;
};
```

---

## 🎯 使用场景

### 场景 1: 实时视频分析

```cpp
// 1. 创建算法实例
auto algorithm = AlgorithmFactory::Create("yolov5", config);

// 2. 在视频帧回调中处理
void onFrameDecoded(DecodedFrame&& frame) {
    algorithm->ProcessAsync(std::move(frame), 
        [](AlgorithmResult&& result) {
            // 处理检测结果
            for (const auto& box : result.detection.boxes) {
                drawBox(box);
            }
        });
}
```

### 场景 2: 批量图片处理

```cpp
// 同步批量处理
std::vector<AlgorithmResult> results;
for (const auto& image : images) {
    auto frame = decodeImage(image);
    results.push_back(algorithm->Process(std::move(frame)));
}
```

### 场景 3: 多算法并行

```cpp
// 同时运行多个算法
auto yolo = AlgorithmFactory::Create("yolov5", config);
auto classifier = AlgorithmFactory::Create("resnet", config);

// 并行处理
auto detection = yolo->ProcessAsync(frame, callback1);
auto classification = classifier->ProcessAsync(frame, callback2);
```

---

## 📊 性能指标

### Inference 模块（当前）

| 模型 | 平台 | 延迟 | 吞吐量 |
|------|------|------|--------|
| YOLOv5s | Intel i9-13900K | ~15ms | ~65 FPS |
| YOLOv5m | Intel i9-13900K | ~30ms | ~33 FPS |
| YOLOv5l | Intel i9-13900K | ~50ms | ~20 FPS |

### 预期性能（完整流水线）

| 组件 | CPU | GPU |
|------|-----|-----|
| Preprocess | ~2ms | ~0.5ms |
| Inference | ~15ms | ~3ms |
| Postprocess | ~1ms | ~0.2ms |
| **总计** | **~18ms** | **~3.7ms** |

---

## 🔧 开发指南

### 添加新算法

1. **实现 IAlgorithm 接口**

```cpp
class MyCustomAlgorithm : public IAlgorithm {
public:
    bool Initialize(const AlgorithmConfig& config) override;
    AlgorithmResult Process(DecodedFrame&& frame) override;
    // ...
};
```

2. **注册到工厂**

```cpp
AlgorithmFactory::Register("my_algorithm", 
    [](const AlgorithmConfig& config) {
        return std::make_unique<MyCustomAlgorithm>();
    });
```

3. **使用**

```cpp
auto algo = AlgorithmFactory::Create("my_algorithm", config);
```

### 性能优化建议

1. **启用异步模式**
   ```cpp
   config.async_mode = true;
   config.num_requests = 4;
   ```

2. **使用 INT8 量化**
   ```bash
   mo --input_model model.onnx --data_type INT8
   ```

3. **批量处理**
   ```cpp
   auto results = engine->InferBatch(inputs);
   ```

4. **复用缓冲区**
   ```cpp
   std::vector<float> buffer;  // 预分配
   // 每次推理复用
   ```

---

## 📚 相关文档

- [VideoPipeline 接口设计](../../docs/VIDEOPIPELINE_INTERFACE_DESIGN.md)
- [零拷贝架构](../../algorithm/CPP_ZERO_COPY_ARCHITECTURE.md)
- [算法迁移计划](../../docs/ALGORITHM_MIGRATION_PLAN.md)
- [Inference 模块文档](inference/README.md)

---

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！

### 贡献流程

1. Fork 项目
2. 创建功能分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 开启 Pull Request

### 代码规范

- 遵循 C++20 标准
- 使用 spdlog 进行日志记录
- 编写单元测试
- 更新文档

---

## 📞 联系方式

- **项目主页**: `d:\file_mx\aaaaa\learncpp`
- **问题反馈**: GitHub Issues
- **文档**: 查看各模块的 README.md

---

## 📄 许可证

本项目采用 MIT 许可证 - 详见 [LICENSE](../../LICENSE) 文件

---

**版本**: v1.0  
**最后更新**: 2026-05-03  
**状态**: Inference 模块已完成，其他模块开发中
