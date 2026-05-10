# PrePostProcessor 颜色格式配置修复

## 🐛 问题描述

之前的实现中，PrePostProcessor **将所有输入格式都转换为 RGB**，没有考虑模型可能期望 BGR 格式的情况。这会导致：

1. **YOLOv5/YOLOv8 模型**（期望 RGB）：✅ 正常工作
2. **OpenCV 训练的模型**（期望 BGR）：❌ 推理结果错误
3. **其他使用 BGR 的模型**：❌ 推理结果错误

### 示例问题

```cpp
// 之前的代码 - 硬编码为 RGB
input_info.preprocess().convert_color(ov::preprocess::ColorFormat::RGB);
```

如果模型是用 OpenCV 训练的（BGR 格式），传入 RGB 数据会导致检测结果完全错误！

---

## ✅ 解决方案

### 1. 添加配置项

在 `PreProcessConfig` 中添加 `model_expected_format` 字段：

```cpp
struct PreProcessConfig {
    ImageFormat input_format = ImageFormat::BGR;           // 输入格式
    ImageFormat model_expected_format = ImageFormat::RGB;  // ⚠️ 新增：模型期望格式
    // ... 其他配置
};
```

### 2. 动态颜色转换

修改 `SetupColorConversion` 函数，根据模型期望的格式决定输出：

```cpp
void PrePostProcessor::SetupColorConversion(ov::preprocess::PrePostProcessor& ppp) {
    // 确定目标颜色格式
    ov::preprocess::ColorFormat target_format;
    if (config_.model_expected_format == ImageFormat::BGR) {
        target_format = ov::preprocess::ColorFormat::BGR;
    } else {
        target_format = ov::preprocess::ColorFormat::RGB;
    }
    
    // 根据输入格式和目柕格式进行转换
    switch (config_.input_format) {
        case ImageFormat::BGR:
            if (target_format == ImageFormat::BGR) {
                // BGR -> BGR (无需转换)
            } else {
                // BGR -> RGB (需要转换)
                input_info.preprocess().convert_color(ov::preprocess::ColorFormat::RGB);
            }
            break;
            
        case ImageFormat::NV12:
            // NV12 -> 目标格式
            input_info.preprocess().convert_color(target_format);
            break;
            
        // ... 其他格式
    }
}
```

### 3. 更新 VideoPipeline 配置

在 `OpenVINOBackend::initialize()` 中明确指定：

```cpp
engine_config.preprocess_config.input_format = ImageFormat::YUV420P;
engine_config.preprocess_config.model_expected_format = ImageFormat::RGB;  // YOLOv5 期望 RGB
```

---

## 📊 颜色转换矩阵

| 输入格式 | 模型期望 RGB | 模型期望 BGR |
|---------|-------------|-------------|
| **RGB** | 无需转换 | RGB → BGR |
| **BGR** | BGR → RGB | 无需转换 |
| **NV12** | NV12 → RGB | NV12 → BGR |
| **NV21** | NV21 → RGB* | NV21 → BGR* |
| **YUV420P** | YUV420P → RGB | YUV420P → BGR |
| **GRAY** | 无颜色转换 | 无颜色转换 |

\* NV21 在 OpenVINO 中近似为 NV12 处理

---

## 🎯 常见模型的配置

### YOLOv5 / YOLOv8

```cpp
config.preprocess_config.input_format = ImageFormat::YUV420P;  // 或 NV12
config.preprocess_config.model_expected_format = ImageFormat::RGB;  // ← YOLO 期望 RGB
```

### OpenCV DNN 模型

```cpp
config.preprocess_config.input_format = ImageFormat::BGR;
config.preprocess_config.model_expected_format = ImageFormat::BGR;  // ← OpenCV 期望 BGR
```

### TensorFlow/PyTorch 模型

```cpp
// 取决于训练时的预处理方式
// 通常使用 RGB
config.preprocess_config.model_expected_format = ImageFormat::RGB;
```

---

## 🔍 如何确定模型期望的格式

### 方法 1：查看模型文档

大多数模型文档会说明输入格式要求。

### 方法 2：检查训练代码

```python
# YOLOv5 训练代码（使用 RGB）
img = img[:, :, ::-1]  # BGR -> RGB

# OpenCV DNN（使用 BGR）
img = cv2.imread('image.jpg')  # 默认 BGR
```

### 方法 3：测试两种格式

如果不确定，可以测试两种配置，看哪种检测结果更准确。

---

## 📝 使用示例

### 示例 1：YOLOv5 + YUV420P

```cpp
InferenceConfig config;
config.model_path = "yolov5s.xml";
config.enable_preprocessor = true;

// 解码器输出 YUV420P，YOLOv5 期望 RGB
config.preprocess_config.input_format = ImageFormat::YUV420P;
config.preprocess_config.model_expected_format = ImageFormat::RGB;

auto engine = InferenceEngineFactory::Create(InferenceEngineType::OPENVINO_CPU);
engine->LoadModel(config);
```

### 示例 2：OpenCV 模型 + BGR

```cpp
InferenceConfig config;
config.model_path = "opencv_model.xml";
config.enable_preprocessor = true;

// OpenCV 图像是 BGR，模型也期望 BGR
config.preprocess_config.input_format = ImageFormat::BGR;
config.preprocess_config.model_expected_format = ImageFormat::BGR;  // ← 关键配置

auto engine = InferenceEngineFactory::Create(InferenceEngineType::OPENVINO_CPU);
engine->LoadModel(config);
```

### 示例 3：NV12 + YOLOv8

```cpp
InferenceConfig config;
config.model_path = "yolov8n.xml";
config.enable_preprocessor = true;

// Intel QSV 解码输出 NV12，YOLOv8 期望 RGB
config.preprocess_config.input_format = ImageFormat::NV12;
config.preprocess_config.model_expected_format = ImageFormat::RGB;

auto engine = InferenceEngineFactory::Create(InferenceEngineType::OPENVINO_CPU);
engine->LoadModel(config);
```

---

## ⚠️ 注意事项

### 1. 默认值

`model_expected_format` 的默认值是 `ImageFormat::RGB`，这适用于大多数现代深度学习模型（YOLO、TensorFlow、PyTorch）。

### 2. 性能影响

颜色转换的性能影响很小（< 1ms），因为这是在 GPU/CPU 加速下完成的。

### 3. 调试日志

启用 DEBUG 日志可以看到实际的颜色转换过程：

```
[DEBUG] Model expects RGB format
[DEBUG] YUV420P -> RGB
```

或

```
[DEBUG] Model expects BGR format
[DEBUG] BGR -> BGR (no conversion)
```

---

## 🧪 验证方法

### 1. 检查日志

运行程序时查看日志输出，确认颜色转换正确：

```
[OpenVINOBackend] PrePostProcessor enabled:
  Input format: YUV420P/NV12/NV21 (auto-detected)
  Model expects: RGB  ← 确认这里正确
```

### 2. 测试结果

使用已知图像测试，比较检测结果是否合理。

### 3. 性能测试

确保推理时间正常（不应该有明显增加）。

---

## 📚 相关文档

- [PrePostProcessor 使用指南](PREPOST_PROCESSOR_USAGE.md)
- [PrePostProcessor 快速参考](PREPOST_PROCESSOR_QUICKREF.md)
- [VideoPipeline 集成指南](../../videopipeline/docs/PREPOST_PROCESSOR_INTEGRATION.md)

---

## 🎉 总结

通过这次修复：

1. ✅ **支持 RGB 和 BGR 模型**：不再硬编码为 RGB
2. ✅ **灵活的配置**：通过 `model_expected_format` 控制
3. ✅ **自动优化**：如果输入和输出格式相同，跳过转换
4. ✅ **详细的日志**：方便调试和验证

现在 PrePostProcessor 可以正确处理各种颜色格式的模型了！🚀
