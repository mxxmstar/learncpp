# PrePostProcessor 使用指南

## 概述

PrePostProcessor 模块利用 OpenVINO 的 PrePostProcessor API，在推理引擎内部自动完成图像预处理，包括：

- ✅ **颜色空间转换**：RGB/BGR/NV12/NV21/YUV420P → RGB
- ✅ **缩放**：任意尺寸 → 模型期望尺寸（如 640x640）
- ✅ **归一化**：UINT8 [0,255] → FLOAT32 [0,1]
- ✅ **布局转换**：NHWC ↔ NCHW
- ✅ **数据类型转换**：UINT8 → FLOAT32

**优势**：所有操作在 OpenVINO 内部完成，避免额外的内存拷贝和 CPU 计算，实现真正的零拷贝预处理。

---

## 快速开始

### 1. 基本用法（启用 PrePostProcessor）

```cpp
#include "alg/inference/i_inference_engine.h"
#include "alg/inference/inference_engine_factory.h"

// 创建推理引擎
auto engine = InferenceEngineFactory::Create(InferenceEngineType::OPENVINO_CPU);

// 配置推理参数
InferenceConfig config;
config.model_path = "yolov5s.xml";
config.device = "CPU";
config.async_mode = true;
config.num_requests = 4;

// ✅ 启用 PrePostProcessor
config.enable_preprocessor = true;
config.preprocess_config.input_format = ImageFormat::NV12;  // 输入是 NV12 格式
config.preprocess_config.model_expected_format = ImageFormat::RGB;  // ⚠️ 重要：模型期望的格式
config.preprocess_config.target_size = {640, 640};           // 缩放到 640x640
config.preprocess_config.normalize = true;                   // 归一化到 [0, 1]
config.preprocess_config.mean = {0.0f, 0.0f, 0.0f};         // 均值
config.preprocess_config.std = {255.0f, 255.0f, 255.0f};    // 标准差（除以 255）
config.preprocess_config.output_layout = "NCHW";             // 输出布局
config.preprocess_config.output_type = "f32";                // 输出类型

// 加载模型
if (!engine->LoadModel(config)) {
    std::cerr << "Failed to load model" << std::endl;
    return -1;
}

// ✅ 现在可以直接传入原始 YUV/NV12 数据，无需手动预处理！
TensorData tensor = TensorData::FromRawData(
    nv12_data,              // NV12 数据指针
    nv12_size,              // 数据大小
    {1, height, width},     // 形状 [N, H, W]
    TensorDataType::UINT8   // 数据类型
);

auto output = engine->Infer(tensor);
```

**⚠️ 重要提示**：`model_expected_format` 必须与模型训练时使用的颜色格式一致！
- **YOLOv5/YOLOv8**：通常使用 **RGB**
- **OpenCV 训练的模型**：可能使用 **BGR**
- **不确定时**：查看模型文档或测试两种格式

### 2. 支持的输入格式

```cpp
// RGB 三通道
config.preprocess_config.input_format = ImageFormat::RGB;

// BGR 三通道（OpenCV 默认）
config.preprocess_config.input_format = ImageFormat::BGR;

// NV12（Y + UV 交错）
config.preprocess_config.input_format = ImageFormat::NV12;

// NV21（Y + VU 交错）
config.preprocess_config.input_format = ImageFormat::NV21;

// YUV420P（Y + U + V 三个独立平面）
config.preprocess_config.input_format = ImageFormat::YUV420P;

// 灰度单通道
config.preprocess_config.input_format = ImageFormat::GRAY;
```

### 3. VideoPipeline 集成示例

```cpp
// 在 OpenVINOBackend 中配置
void OpenVINOBackend::initialize() {
    InferenceConfig config;
    config.model_path = model_path_;
    config.device = device_;
    
    // 启用 PrePostProcessor
    config.enable_preprocessor = true;
    config.preprocess_config.input_format = ImageFormat::NV12;  // 解码器输出 NV12
    config.preprocess_config.target_size = {640, 640};
    config.preprocess_config.normalize = true;
    config.preprocess_config.mean = {0.0f, 0.0f, 0.0f};
    config.preprocess_config.std = {255.0f, 255.0f, 255.0f};
    
    engine_ = InferenceEngineFactory::Create(InferenceEngineType::OPENVINO_CPU);
    engine_->LoadModel(config);
}

void OpenVINOBackend::processFrame(const VideoFrame& frame) {
    // ✅ 直接传入原始 NV12 数据，OpenVINO 自动处理所有预处理
    size_t nv12_size = frame.width * frame.height * 3 / 2;
    
    auto tensor = TensorData::FromRawData(
        frame.data[0],                    // NV12 数据
        nv12_size,
        {1, frame.height, frame.width},   // [N, H, W]
        TensorDataType::UINT8
    );
    
    auto output = engine_->Infer(tensor);
    // ... 处理结果
}
```

---

## 性能对比

### 传统方式（手动预处理）

```
YUV420P → (OpenCV) → BGR → (OpenCV) → Resize → (OpenCV) → RGB 
→ (Manual) → FLOAT32 → (Manual) → Normalize → OpenVINO Infer
```

- ❌ 多次内存拷贝
- ❌ CPU 密集计算
- ❌ 额外的依赖（OpenCV）

### PrePostProcessor 方式

```
YUV420P/NV12/NV21/RGB/BGR → OpenVINO PrePostProcessor → Infer
```

- ✅ 零拷贝（或最小拷贝）
- ✅ GPU/CPU 加速（取决于设备）
- ✅ 无额外依赖

---

## 注意事项

### 1. 内存布局

对于多平面格式（如 YUV420P），确保数据在内存中是连续的：

```cpp
// ✅ 正确：Y + U + V 连续存储
uint8_t* yuv_data = new uint8_t[y_size + u_size + v_size];
// Y plane: yuv_data[0 .. y_size-1]
// U plane: yuv_data[y_size .. y_size+u_size-1]
// V plane: yuv_data[y_size+u_size .. y_size+u_size+v_size-1]

auto tensor = TensorData::FromRawData(yuv_data, total_size, shape, dtype);
```

### 2. 归一化参数

YOLOv5 通常使用简单的归一化（除以 255）：

```cpp
config.preprocess_config.mean = {0.0f, 0.0f, 0.0f};
config.preprocess_config.std = {255.0f, 255.0f, 255.0f};
```

其他模型可能需要不同的参数，请参考模型文档。

### 3. 兼容性

PrePostProcessor 需要 OpenVINO 2021.4 或更高版本。

---

## 故障排查

### 问题 1：PrePostProcessor 配置失败

**症状**：日志显示 `Failed to configure PrePostProcessor`

**解决方案**：
1. 检查 OpenVINO 版本是否支持 PrePostProcessor
2. 确认模型输入形状与配置匹配
3. 查看详细错误日志

### 问题 2：推理结果不正确

**症状**：推理成功但检测结果错误

**解决方案**：
1. 检查颜色空间转换是否正确（RGB vs BGR）
2. 验证归一化参数（mean/std）
3. 确认输入数据格式与配置一致

---

## 完整示例

参见：
- `modules/alg/test/test_prepost_processor.cpp` - PrePostProcessor 单元测试
- `modules/videopipeline/include/videopipeline/backends/openvino_backend.h` - VideoPipeline 集成示例

---

## 参考资料

- [OpenVINO Preprocessing API](https://docs.openvino.ai/latest/preprocessing_api.html)
- [OpenVINO Color Format Conversion](https://docs.openvino.ai/latest/ov_preprocessing_color_format.html)
