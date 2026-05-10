# YUV 格式形状配置指南

## 🐛 问题背景

在使用 OpenVINO PrePostProcessor 处理 YUV 格式时，遇到了以下错误：

```
Shape inference input shapes {[1,4,640,1]}
Image height shall be divisible by 3
```

这是因为 **YUV 格式的形状定义不正确**。

---

## ✅ 正确的形状定义

### YUV420P (I420)

YUV420P 是**单平面格式**，内存布局为：

```
[Y plane: H × W]
[U plane: (H/2) × (W/2)]
[V plane: (H/2) × (W/2)]
```

**总大小**: `H × W × 1.5`

**OpenVINO 期望的形状**: `[N, H*3/2, W]`

```cpp
// ✅ 正确
int64_t total_height = frame.height * 3 / 2;  // H * 1.5
auto tensor = TensorData::FromRawData(
    frame.data[0],
    total_size,
    {1, total_height, frame.width},  // [N, H*3/2, W]
    TensorDataType::UINT8
);

// ❌ 错误
auto tensor = TensorData::FromRawData(
    frame.data[0],
    total_size,
    {1, frame.height, frame.width},  // [N, H, W] - 错误！
    TensorDataType::UINT8
);
```

**示例**：
- 输入分辨率：1920×1080
- Y 平面：1920×1080 = 2,073,600 字节
- U 平面：960×540 = 518,400 字节
- V 平面：960×540 = 518,400 字节
- **总大小**：3,110,400 字节
- **形状**：`[1, 1620, 1920]` （1080 × 1.5 = 1620）

---

### NV12

NV12 也是**单平面格式**，内存布局为：

```
[Y plane: H × W]
[UV interleaved: (H/2) × W]
```

**总大小**: `H × W × 1.5`

**OpenVINO 期望的形状**: `[N, H*3/2, W]`（与 YUV420P 相同）

```cpp
size_t total_size = frame.width * frame.height * 3 / 2;
int64_t total_height = frame.height * 3 / 2;

auto tensor = TensorData::FromRawData(
    frame.data[0],
    total_size,
    {1, total_height, frame.width},  // [N, H*3/2, W]
    TensorDataType::UINT8
);
```

---

### NV21

NV21 与 NV12 类似，只是 UV 顺序相反。

**形状**: `[N, H*3/2, W]`（与 NV12 相同）

```cpp
size_t total_size = frame.width * frame.height * 3 / 2;
int64_t total_height = frame.height * 3 / 2;

auto tensor = TensorData::FromRawData(
    frame.data[0],
    total_size,
    {1, total_height, frame.width},  // [N, H*3/2, W]
    TensorDataType::UINT8
);
```

---

## 📊 形状对比表

| 格式 | 内存布局 | 总大小 | OpenVINO 形状 | 说明 |
|------|---------|--------|--------------|------|
| **YUV420P** | Y + U + V | H×W×1.5 | `[N, H*3/2, W]` | 三个独立平面 |
| **NV12** | Y + UV | H×W×1.5 | `[N, H*3/2, W]` | UV 交错 |
| **NV21** | Y + VU | H×W×1.5 | `[N, H*3/2, W]` | VU 交错 |
| **RGB** | R,G,B 交错 | H×W×3 | `[N, H, W, 3]` 或 `[N, 3, H, W]` | 三通道 |
| **BGR** | B,G,R 交错 | H×W×3 | `[N, H, W, 3]` 或 `[N, 3, H, W]` | 三通道 |

---

## 🔍 为什么高度要乘以 1.5？

### YUV420P 的内存布局

对于 1920×1080 的图像：

```
内存地址: 0                    2,073,600        2,592,000      3,110,400
         |---------------------|----------------|----------------|
         |   Y plane           |   U plane      |   V plane      |
         |   1920×1080         |   960×540      |   960×540      |
         |   2,073,600 bytes   |   518,400      |   518,400      |
```

OpenVINO 将整个数据视为一个**单通道图像**，高度为：
```
total_height = H + H/4 + H/4 = H × 1.5 = 1080 × 1.5 = 1620
```

所以形状是 `[1, 1620, 1920]`。

### 验证公式

```
total_height × width = 1620 × 1920 = 3,110,400
total_size = 1920 × 1080 × 1.5 = 3,110,400
✅ 匹配！
```

---

## 💡 代码示例

### VideoPipeline 中的正确实现

```cpp
if (frame.format == 0) {  // AV_PIX_FMT_YUV420P
    // ✅ 正确：高度乘以 1.5
    size_t y_size = static_cast<size_t>(frame.width) * frame.height;
    size_t uv_size = y_size / 4;
    size_t total_size = y_size + 2 * uv_size;
    int64_t total_height = static_cast<int64_t>(frame.height * 3 / 2);
    
    auto tensor = TensorData::FromRawData(
        frame.data[0],
        total_size,
        {1, total_height, static_cast<int64_t>(frame.width)},  // [N, H*3/2, W]
        TensorDataType::UINT8
    );
}
```

### 常见错误

```cpp
// ❌ 错误 1：使用原始高度
{1, frame.height, frame.width}  // [1, 1080, 1920] - 太小！

// ❌ 错误 2：使用三通道形状
{1, 3, frame.height, frame.width}  // [1, 3, 1080, 1920] - 错误！

// ✅ 正确：使用 1.5 倍高度
{1, frame.height * 3 / 2, frame.width}  // [1, 1620, 1920]
```

---

## 🧪 验证方法

### 1. 检查总大小

```cpp
size_t expected_size = width * height * 3 / 2;
size_t actual_size = tensor.size_bytes;

if (expected_size != actual_size) {
    std::cerr << "Size mismatch!" << std::endl;
}
```

### 2. 检查形状

```cpp
auto shape = tensor.shape;
int64_t expected_height = height * 3 / 2;

if (shape[1] != expected_height) {
    std::cerr << "Height mismatch! Expected: " << expected_height 
              << ", Got: " << shape[1] << std::endl;
}
```

### 3. 运行测试

```bash
cd modules/videopipeline/test/bin
./test_video_pipeline_openvino http://127.0.0.1:8888/live/proxy_cam1.live.flv yolov5s.xml CPU 60
```

查看日志确认没有形状推断错误。

---

## 📚 OpenVINO 文档参考

- [I420 Shape Inference](https://docs.openvino.ai/latest/ov_preprocessing_color_format.html)
- [Color Format Conversion](https://docs.openvino.ai/latest/ov_preprocessing_i420.html)

关键引用：
> "For I420 format, the input tensor should have shape [N, H*3/2, W] where H is the original image height."

---

## ⚠️ 注意事项

### 1. 高度必须能被 2 整除

YUV420P 要求原始高度能被 2 整除（因为 U/V 平面是半采样）。

```cpp
if (frame.height % 2 != 0) {
    LOG_MAIN_WARN_AT("Height {} is not divisible by 2, may cause issues", frame.height);
}
```

### 2. 宽度必须能被 2 整除

同样，宽度也必须能被 2 整除。

### 3. 内存对齐

确保 YUV 数据在内存中是连续的，没有填充字节。

---

## 🎯 总结

| 要点 | 说明 |
|------|------|
| **YUV420P 形状** | `[N, H*3/2, W]` |
| **NV12/NV21 形状** | `[N, H*3/2, W]` |
| **总大小** | `H × W × 1.5` |
| **原因** | Y + U + V = H×W + H/4×W/2 + H/4×W/2 = H×W×1.5 |
| **常见错误** | 使用 `[N, H, W]` 导致形状推断失败 |

记住：**YUV 格式的高度要乘以 1.5！** 🎯
