# PrePostProcessor 快速参考

## 🚀 一行代码启用

```cpp
config.enable_preprocessor = true;
```

---

## 📝 配置模板

### YOLOv5 + NV12（推荐）

```cpp
InferenceConfig config;
config.model_path = "yolov5s.xml";
config.enable_preprocessor = true;

// 预处理配置
config.preprocess_config.input_format = ImageFormat::NV12;  // 解码器输出
config.preprocess_config.target_size = {640, 640};           // YOLOv5 输入尺寸
config.preprocess_config.normalize = true;
config.preprocess_config.mean = {0.0f, 0.0f, 0.0f};
config.preprocess_config.std = {255.0f, 255.0f, 255.0f};    // 归一化到 [0,1]
config.preprocess_config.output_layout = "NCHW";
config.preprocess_config.output_type = "f32";
```

### OpenCV BGR

```cpp
config.preprocess_config.input_format = ImageFormat::BGR;
config.preprocess_config.target_size = {640, 640};
config.preprocess_config.normalize = true;
config.preprocess_config.std = {255.0f, 255.0f, 255.0f};
```

### RGB 图像

```cpp
config.preprocess_config.input_format = ImageFormat::RGB;
config.preprocess_config.target_size = {224, 224};  // 例如 ResNet
config.preprocess_config.mean = {123.675f, 116.28f, 103.53f};   // ImageNet 均值
config.preprocess_config.std = {58.395f, 57.12f, 57.375f};      // ImageNet 标准差
```

---

## 🎯 支持的格式

| 格式 | 枚举值 | 用途 |
|------|--------|------|
| RGB | `ImageFormat::RGB` | 标准 RGB 图像 |
| BGR | `ImageFormat::BGR` | OpenCV 默认格式 |
| NV12 | `ImageFormat::NV12` | 视频解码常见格式 |
| NV21 | `ImageFormat::NV21` | Android 相机格式 |
| YUV420P | `ImageFormat::YUV420P` | FFmpeg 常见格式 |
| GRAY | `ImageFormat::GRAY` | 灰度图像 |

---

## ⚡ 性能提示

### ✅ 最佳实践

1. **使用 NV12/YUV420P**：避免额外的颜色空间转换
2. **启用异步模式**：`config.async_mode = true`
3. **多推理请求**：`config.num_requests = 4`

### ❌ 避免

1. 不要手动进行 YUV→RGB 转换
2. 不要手动缩放图像
3. 不要手动归一化

---

## 🔍 故障排查

### 问题：PrePostProcessor 配置失败

**检查清单**：
- [ ] OpenVINO 版本 ≥ 2021.4
- [ ] 模型文件路径正确
- [ ] 输入格式与数据匹配

### 问题：推理结果错误

**检查清单**：
- [ ] 颜色格式正确（RGB vs BGR）
- [ ] 归一化参数正确（mean/std）
- [ ] 目标尺寸与模型匹配

---

## 📊 性能对比

| 方法 | 预处理时间 | 内存拷贝 | 依赖 |
|------|-----------|---------|------|
| 手动（OpenCV） | 10-20ms | 3-4 次 | OpenCV |
| PrePostProcessor | 2-5ms | 0-1 次 | 无 |

**提升：3-5 倍** 🚀

---

## 💡 示例代码

### VideoPipeline 集成

```cpp
void OpenVINOBackend::initialize() {
    InferenceConfig config;
    config.model_path = model_path_;
    config.enable_preprocessor = true;
    config.preprocess_config.input_format = ImageFormat::NV12;
    config.preprocess_config.target_size = {640, 640};
    config.preprocess_config.std = {255.0f, 255.0f, 255.0f};
    
    engine_->LoadModel(config);
}

void OpenVINOBackend::processFrame(const VideoFrame& frame) {
    size_t nv12_size = frame.width * frame.height * 3 / 2;
    
    auto tensor = TensorData::FromRawData(
        frame.data[0],
        nv12_size,
        {1, frame.height, frame.width},
        TensorDataType::UINT8
    );
    
    auto output = engine_->Infer(tensor);
    // ... 处理结果
}
```

---

## 📚 更多信息

- 详细文档：`PREPOST_PROCESSOR_USAGE.md`
- 实现总结：`PREPOST_PROCESSOR_SUMMARY.md`
- 测试代码：`test/test_prepost_processor.cpp`
