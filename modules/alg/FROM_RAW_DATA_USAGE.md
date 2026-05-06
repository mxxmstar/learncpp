# FromRawData API 使用指南

## 📖 概述

由于模块解耦的需要，`TensorData` 不再直接依赖 `VideoFrame`，而是提供了更通用的 `FromRawData()` 方法。

---

## 🚀 基本用法

### 从 VideoFrame 创建 TensorData

```cpp
#include "alg/inference/tensor_data.h"
#include "decoder/i_decoder.h"  // VideoFrame 定义

void process_frame(VideoFrame& frame, IInferenceEngine& engine) {
    // ✅ 使用 FromRawData 创建零拷贝张量
    auto tensor = TensorData::FromRawData(
        frame.data[0],           // Y 平面数据指针
        frame.linesize[0],       // Y 平面每行字节数
        frame.height,            // 高度
        {1, 1, frame.height, frame.width},  // [N, C, H, W]
        TensorDataType::UINT8    // 数据类型
    );
    
    // 执行推理
    auto output = engine.Infer(tensor);
}
```

---

## 📊 API 参数说明

```cpp
static TensorData FromRawData(
    const uint8_t* data,              // 数据指针
    int linesize,                     // 每行字节数
    int height,                       // 高度
    const std::vector<int64_t>& shape,// 形状 [N, C, H, W]
    TensorDataType dtype = TensorDataType::UINT8  // 数据类型
);
```

### 参数详解

| 参数 | 说明 | 示例 |
|------|------|------|
| `data` | 数据指针，通常是 `VideoFrame::data[0]`（Y 平面） | `frame.data[0]` |
| `linesize` | 每行字节数，通常是 `VideoFrame::linesize[0]` | `frame.linesize[0]` |
| `height` | 图像高度 | `frame.height` |
| `shape` | 张量形状 `[N, C, H, W]` | `{1, 3, 480, 640}` |
| `dtype` | 数据类型 | `TensorDataType::UINT8` |

---

## 💡 使用示例

### 示例 1: 单通道灰度图像

```cpp
// YUV420P 的 Y 平面（灰度）
auto tensor = TensorData::FromRawData(
    frame.data[0],                    // Y 平面
    frame.linesize[0],                // Y 平面 stride
    frame.height,                     // 高度
    {1, 1, frame.height, frame.width}, // [N=1, C=1, H, W]
    TensorDataType::UINT8
);
```

---

### 示例 2: 三通道 YUV 图像

```cpp
// YUV420P 完整数据（Y + U/2 + V/2）
auto tensor = TensorData::FromRawData(
    frame.data[0],                    // Y 平面起始
    frame.linesize[0],                // Y 平面 stride
    frame.height,                     // 高度
    {1, 3, frame.height, frame.width}, // [N=1, C=3, H, W]
    TensorDataType::UINT8
);

// 注意：实际内存布局是 YUV420P，不是 RGB
// OpenVINO 会根据模型需要进行转换
```

---

### 示例 3: 与 Puller + Decoder 集成

```cpp
#include "puller/zlm/zlm_httpflv_puller.h"
#include "decoder/ffmpeg_decoder.h"
#include "alg/inference/inference_engine_factory.h"

class VideoInferencePipeline {
public:
    void start(const std::string& stream_url) {
        // 初始化模块
        auto puller = std::make_unique<ZLMHttpFlvPuller>(io_ctx_);
        auto decoder = std::make_unique<FfmpegDecoder>();
        
        InferenceConfig config;
        config.model_path = "models/yolov5s.xml";
        config.device = "CPU";
        auto engine = InferenceEngineFactory::Create("openvino_cpu", config);
        
        // 启动拉流
        puller->start(
            stream_url,
            // 序列头回调
            [&decoder](int codec_id, const uint8_t* data, int size) {
                decoder->Open(data, size, codec_id);
            },
            // 帧回调
            [&decoder, &engine](const uint8_t* nalu, int size, int64_t pts) {
                decoder->Decode(nalu, size, pts, 
                    [&engine](VideoFrame&& frame) {
                        // ✅ 零拷贝创建 TensorData
                        auto tensor = TensorData::FromRawData(
                            frame.data[0],
                            frame.linesize[0],
                            frame.height,
                            {1, 3, frame.height, frame.width},
                            TensorDataType::UINT8
                        );
                        
                        // 推理
                        auto output = engine->Infer(tensor);
                        
                        if (output.success) {
                            process_result(output);
                        }
                    });
            }
        );
    }
    
private:
    void process_result(const InferenceOutput& output) {
        // 处理推理结果
    }
    
    boost::asio::io_context io_ctx_;
};
```

---

### 示例 4: 异步推理

```cpp
void async_inference(VideoFrame& frame, IInferenceEngine& engine) {
    // 使用 shared_ptr 保持 frame 存活
    auto frame_ptr = std::make_shared<VideoFrame>(std::move(frame));
    
    auto tensor = TensorData::FromRawData(
        frame_ptr->data[0],
        frame_ptr->linesize[0],
        frame_ptr->height,
        {1, 3, frame_ptr->height, frame_ptr->width},
        TensorDataType::UINT8
    );
    
    // 异步推理
    engine.InferAsync(tensor, [frame_ptr](InferenceOutput output) {
        // frame_ptr 保持 frame 存活直到回调完成
        if (output.success) {
            process_result(output);
        }
    });
}
```

---

## ⚠️ 注意事项

### 1. 生命周期管理

**最重要**: 必须确保数据指针在推理完成前有效。

```cpp
// ❌ 错误：frame 在推理完成前被销毁
{
    VideoFrame frame = decode_packet(...);
    auto tensor = TensorData::FromRawData(frame.data[0], ...);
}  // ← frame 销毁，tensor.data 成为悬空指针！
auto output = engine->Infer(tensor);  // 💥 Crash!

// ✅ 正确：同步推理
{
    VideoFrame frame = decode_packet(...);
    auto tensor = TensorData::FromRawData(frame.data[0], ...);
    auto output = engine->Infer(tensor);  // frame 仍然有效
}  // frame 在此处销毁

// ✅ 正确：异步推理使用 shared_ptr
auto frame_ptr = std::make_shared<VideoFrame>(decode_packet(...));
auto tensor = TensorData::FromRawData(frame_ptr->data[0], ...);
engine.InferAsync(tensor, [frame_ptr](auto output) {
    // frame_ptr 保持 frame 存活
});
```

---

### 2. 数据格式

不同的模型可能需要不同的输入格式：

```cpp
// YOLOv5: 通常需要 RGB Float [1, 3, 640, 640]
// 但我们可以传递 YUV UINT8，让 OpenVINO 内部转换
auto tensor = TensorData::FromRawData(
    frame.data[0],
    frame.linesize[0],
    frame.height,
    {1, 3, 640, 640},  // 注意：这里指定目标形状
    TensorDataType::UINT8
);

// 如果模型接受 YUV，直接使用原始尺寸
auto tensor_yuv = TensorData::FromRawData(
    frame.data[0],
    frame.linesize[0],
    frame.height,
    {1, 3, frame.height, frame.width},  // 原始尺寸
    TensorDataType::UINT8
);
```

**建议**: 检查模型的输入要求：

```cpp
auto input_info = engine->GetInputInfo();
for (const auto& info : input_info) {
    std::cout << "Input: " << info.name 
              << ", Shape: " << info.shape
              << ", Type: " << info.dtype
              << std::endl;
}
```

---

### 3. 内存布局

YUV420P 的内存布局：

```
Y 平面: width x height
U 平面: width/2 x height/2
V 平面: width/2 x height/2

总大小: width * height * 3 / 2
```

`FromRawData` 默认只引用 Y 平面（`data[0]`），如果需要完整的 YUV 数据，需要特殊处理。

---

## 🔧 高级用法

### 1. 自定义预处理后使用

```cpp
void preprocess_and_infer(VideoFrame& frame) {
    // 如果需要缩放，先使用 OpenCV 处理
    cv::Mat yuv(frame.height * 3/2, frame.width, CV_8UC1, frame.data[0]);
    cv::Mat resized;
    cv::resize(yuv, resized, cv::Size(640, 640));
    
    // 然后使用处理后的数据
    auto tensor = TensorData::FromRawData(
        resized.data,              // 缩放后的数据
        resized.step,              // OpenCV 的 step 相当于 linesize
        resized.rows,              // 高度
        {1, 1, 640, 640},
        TensorDataType::UINT8
    );
    
    auto output = engine->Infer(tensor);
}
```

---

### 2. 批量推理

```cpp
void batch_inference(std::vector<VideoFrame>& frames) {
    std::vector<TensorData> tensors;
    
    for (auto& frame : frames) {
        tensors.push_back(TensorData::FromRawData(
            frame.data[0],
            frame.linesize[0],
            frame.height,
            {1, 3, frame.height, frame.width},
            TensorDataType::UINT8
        ));
    }
    
    auto outputs = engine->InferBatch(tensors);
    
    for (size_t i = 0; i < outputs.size(); ++i) {
        process_result(outputs[i]);
    }
}
```

---

## 🐛 常见问题

### Q1: 为什么不用 `FromVideoFrame()`？

**A**: 为了模块解耦。`TensorData` 在 `alg` 模块，`VideoFrame` 在 `decoder` 模块。直接依赖会导致循环依赖或编译问题。

**解决方案**: 使用 `FromRawData()`，传递必要的参数而不是整个结构体。

---

### Q2: 如何访问 U/V 平面？

**A**: `FromRawData` 目前只支持单个数据指针。如果需要多平面数据，可以：

```cpp
// 方法 1: 合并平面（会有拷贝）
std::vector<uint8_t> yuv_data;
yuv_data.insert(yuv_data.end(), frame.data[0], frame.data[0] + y_size);
yuv_data.insert(yuv_data.end(), frame.data[1], frame.data[1] + u_size);
yuv_data.insert(yuv_data.end(), frame.data[2], frame.data[2] + v_size);

auto tensor = TensorData::FromCpuUint8(
    yuv_data.data(),
    shape,
    yuv_data.size()
);

// 方法 2: 分别处理每个平面
auto y_tensor = TensorData::FromRawData(frame.data[0], ...);
auto u_tensor = TensorData::FromRawData(frame.data[1], ...);
auto v_tensor = TensorData::FromRawData(frame.data[2], ...);
```

---

### Q3: 性能如何？

**A**: `FromRawData` 是零拷贝的，性能与原来的 `FromVideoFrame` 相同：

- **耗时**: < 0.1 ms（只是指针赋值和简单计算）
- **内存**: 无额外分配
- **拷贝**: 0 次

---

## 📚 相关文档

- [ZERO_COPY_OPTIMIZATION_PLAN.md](./ZERO_COPY_OPTIMIZATION_PLAN.md) - 技术设计方案
- [ZERO_COPY_USAGE_GUIDE.md](./ZERO_COPY_USAGE_GUIDE.md) - 完整使用指南
- [ARCHITECTURE_COMPARISON.md](./ARCHITECTURE_COMPARISON.md) - 架构对比

---

**更新日期**: 2026-05-04  
**作者**: Lingma AI Assistant  
**版本**: v1.0
