# PrePostProcessor 快速配置清单

## ✅ YUV420P 成功配置（已验证）

### 关键代码

```cpp
// ⚠️ 最关键的一行：必须设置 spatial_static_shape
input_info.tensor()
    .set_element_type(ov::element::u8)
    .set_layout("NHWC")
    .set_spatial_static_shape(h * 3 / 2, w)  // ← 必须有！H*1.5 × W
    .set_color_format(ov::preprocess::ColorFormat::I420_SINGLE_PLANE);
```

---

## 📋 配置检查清单

### 1. 输入 Tensor 配置 ✓

- [ ] 数据类型：`ov::element::u8`
- [ ] 布局：`"NHWC"`
- [ ] 空间尺寸：`set_spatial_static_shape(H*3/2, W)`
- [ ] 颜色格式：`I420_SINGLE_PLANE` / `NV12_SINGLE_PLANE`

### 2. 模型布局配置 ✓

- [ ] 设置为 `"NCHW"` 或 `"NHWC"`
- [ ] 使用 `input.model().set_layout()`

### 3. 颜色转换配置 ✓

- [ ] 确定目标格式（RGB 或 BGR）
- [ ] 调用 `convert_color(target_format)`

### 4. 数据类型转换 ✓

- [ ] UINT8 → FLOAT32
- [ ] 使用 `convert_element_type(ov::element::f32)`

### 5. 缩放配置 ✓

- [ ] 使用 `RESIZE_LINEAR`
- [ ] 目标尺寸从模型自动推断

### 6. 归一化配置 ✓

- [ ] 设置 mean 和 scale
- [ ] 典型值：mean={0,0,0}, scale={255,255,255}

---

## 🔧 常见格式配置

### YUV420P

```cpp
input_info.tensor()
    .set_element_type(ov::element::u8)
    .set_layout("NHWC")
    .set_spatial_static_shape(h * 3 / 2, w)
    .set_color_format(ov::preprocess::ColorFormat::I420_SINGLE_PLANE);
```

**TensorData 形状**：`{1, H*3/2, W}`

### NV12

```cpp
input_info.tensor()
    .set_element_type(ov::element::u8)
    .set_layout("NHWC")
    .set_spatial_static_shape(h * 3 / 2, w)
    .set_color_format(ov::preprocess::ColorFormat::NV12_SINGLE_PLANE);
```

**TensorData 形状**：`{1, H*3/2, W}`

### RGB/BGR

```cpp
input_info.tensor()
    .set_element_type(ov::element::u8)
    .set_layout("NHWC")
    .set_color_format(ov::preprocess::ColorFormat::RGB);  // 或 BGR
```

**TensorData 形状**：`{1, H, W, 3}`

---

## ❌ 常见错误

### 错误 1：缺少 spatial_static_shape

```cpp
// ❌ 失败
.set_color_format(ov::preprocess::ColorFormat::I420_SINGLE_PLANE);

// ✅ 成功
.set_spatial_static_shape(h * 3 / 2, w)
.set_color_format(ov::preprocess::ColorFormat::I420_SINGLE_PLANE);
```

### 错误 2：形状定义错误

```cpp
// ❌ 错误
{1, frame.height, frame.width}

// ✅ 正确
{1, frame.height * 3 / 2, frame.width}
```

### 错误 3：布局错误

```cpp
// ❌ 错误
.set_layout("NCHW")  // YUV 不能用 NCHW

// ✅ 正确
.set_layout("NHWC")  // YUV 必须用 NHWC
```

---

## 🎯 VideoPipeline 配置

```cpp
engine_config.enable_preprocessor = true;
engine_config.preprocess_config.input_format = ImageFormat::YUV420P;
engine_config.preprocess_config.model_expected_format = ImageFormat::RGB;
engine_config.preprocess_config.target_size = {640, 640};
engine_config.preprocess_config.normalize = true;
engine_config.preprocess_config.mean = {0.0f, 0.0f, 0.0f};
engine_config.preprocess_config.std = {255.0f, 255.0f, 255.0f};
engine_config.preprocess_config.layout = "NCHW";
engine_config.preprocess_config.dtype = "f32";
```

---

## 📊 性能对比

| 方法 | 时间 | 拷贝次数 | 推荐度 |
|------|------|---------|--------|
| **PrePostProcessor** | 2-5ms | 0-1 | ⭐⭐⭐⭐⭐ |
| OpenCV 手动 | 10-15ms | 3-4 | ⭐⭐⭐ |

---

## 🔍 验证日志

成功的日志应该包含：

```
✅ Input format: I420_SINGLE_PLANE (YUV420P)
✅ Model layout: NCHW
✅ Color conversion: YUV420P -> RGB
✅ Convert element type: u8 -> f32
✅ PrePostProcessor configured successfully
✅ Model compiled successfully
```

失败的日志会显示：

```
❌ Image height shall be divisible by 3
❌ Shape inference input shapes {[1,4,640,1]}
❌ Failed to configure PrePostProcessor
```

---

## 💡 提示

1. **YUV 格式必须设置 `set_spatial_static_shape()`**
2. **YUV 输入布局必须是 NHWC**
3. **YUV 形状高度要乘以 1.5**
4. **配置顺序很重要**：Input → Layout → Color → Type → Resize → Normalize

---

📖 详细文档：[PREPOST_PROCESSOR_SUCCESS_GUIDE.md](PREPOST_PROCESSOR_SUCCESS_GUIDE.md)
