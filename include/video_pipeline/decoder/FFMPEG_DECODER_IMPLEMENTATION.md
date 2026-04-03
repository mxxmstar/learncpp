# FFmpegDecoder 实现总结

## ✅ 已完成的工作

### 📁 创建的文件

1. **include/video_pipeline/decoder/ffmpeg_decoder.h** - 头文件（84 行）
2. **src/video_pipeline/decoder/ffmpeg_decoder.cpp** - 实现文件（276 行）
3. **test/video_pipeline/test_ffmpeg_decoder.cpp** - 测试文件（237 行）

---

## 🎯 为什么选择 HTTP-FLV？

在实现之前，先回答你的问题：**为什么用 HTTP-FLV 而不用 RTSP/RTMP？**

### 技术对比

| 特性 | HTTP-FLV | RTSP (RTP over TCP) | RTMP |
|------|----------|---------------------|------|
| **延迟** | ~500ms | ~200ms | ~1s |
| **协议复杂度** | ⭐⭐ 简单 | ⭐⭐⭐⭐ 复杂 | ⭐⭐⭐ 中等 |
| **实现难度** | ⭐⭐ 容易 | ⭐⭐⭐⭐ 困难 | ⭐⭐⭐ 中等 |
| **防火墙穿透** | ✅ 容易 (80/443) | ⚠️ 困难 | ✅ 容易 (1935) |
| **ZLMediaKit 适配** | ✅ 完美 | ✅ 支持 | ✅ 支持 |
| **调试难度** | ⭐⭐ 容易 | ⭐⭐⭐⭐ 困难 | ⭐⭐⭐ 中等 |

### 选择 HTTP-FLV 的理由

#### 1. **与 ZLMediaKit 完美集成**
```cpp
// ZLMediaKit 配置简单
rtsp://camera → ZLMediaKit → http://localhost:8080/live/stream.flv
```

#### 2. **协议简单，易于解析**
```cpp
// HTTP-FLV 结构清晰
[FLV Header][Tag1][Tag2]...
// Tag = [Header(11B)][Data][PrevTagSize(4B)]
```

#### 3. **适合本项目场景**
```
你的需求：ZLMediaKit → FFmpeg → OpenCV → 算法
HTTP-FLV 正好匹配这个流水线，延迟可接受 (~500ms)
```

#### 4. **开发效率高**
- HTTP-FLV 解析代码只需几百行
- RTSP 需要处理 SDP 协商、RTP 重组、时间戳同步等

---

## 🔧 FFmpegDecoder 核心功能

### 1. H.264/H.265 解码

```cpp
// 打开解码器（需要 SPS/PPS）
decoder.open(extradata, size, AV_CODEC_ID_H264);

// 解码 NALU
decoder.decode(nalu_data, nalu_size, pts, callback);
```

### 2. YUV → BGR 转换

使用 `libswscale` 进行高效的格式转换：
```cpp
YUV420P → BGR24 (OpenCV Mat)
```

### 3. 多线程解码

```cpp
// 设置解码线程数
decoder.setThreadCount(4);

// FFmpeg 内部并行解码多个帧
```

### 4. 低延迟优化

```cpp
// 启用低延迟模式
codec_ctx_->flags |= AV_CODEC_FLAG_LOW_DELAY;
codec_ctx_->flags2 |= AV_CODEC_FLAG2_FAST;
```

---

## 📋 使用示例

### 基本用法

```cpp
#include "video_pipeline/decoder/ffmpeg_decoder.h"

// 1. 创建解码器
FFmpegDecoder decoder;

// 2. 准备 extradata（SPS/PPS）
std::vector<uint8_t> extradata = createExtradata(sps, pps);

// 3. 打开解码器
bool success = decoder.open(extradata.data(), extradata.size(), 27);

// 4. 解码 NALU
decoder.decode(nalu_data, nalu_size, pts,
    [](cv::Mat&& frame, int64_t pts) {
        // 处理解码后的帧
        cv::imshow("Video", frame);
        cv::waitKey(1);
    });
```

### 与 ZLMPuller 配合使用

```cpp
auto puller = std::make_unique<ZLMPuller>(io_ctx);
auto decoder = std::make_unique<FFmpegDecoder>();

// 拉流回调中解码
puller->start(url, 
    [decoder](const uint8_t* nalu, int size, int64_t pts) {
        decoder->decode(nalu, size, pts,
            [](cv::Mat&& frame, int64_t pts) {
                // 推入队列或直接处理
                queue.push({frame, pts});
            });
    });
```

---

## ⚠️ 注意事项

### 1. Extradata 格式

FFmpeg 需要 AVCC 格式的 extradata：
```cpp
// AVCC 格式：[version][profile][SPS length][SPS][PPS length][PPS]
std::vector<uint8_t> createExtradata(const std::vector<uint8_t>& sps,
                                     const std::vector<uint8_t>& pps) {
    std::vector<uint8_t> data(5 + sps.size() + 3 + pps.size());
    // ... 填充数据
    return data;
}
```

### 2. 内存管理

```cpp
// convertToMat() 返回的是深拷贝的 Mat
// 避免共享 FFmpeg 分配的内存，防止生命周期问题
cv::Mat mat_copy = mat.clone();  // ✅ 安全
```

### 3. 错误处理

```cpp
// 检查返回值
int ret = avcodec_send_packet(...);
if (ret == AVERROR(EAGAIN)) {
    // 需要先从解码器取出帧
} else if (ret < 0) {
    // 真正的错误
}
```

---

## 🐛 常见问题

### Q1: 解码失败 "Invalid data found"

**A:** 通常是 NALU 格式问题
```cpp
// 确保 NALU 是 Annex B 格式（带起始码）
// 如果是 AVCC 格式（带长度前缀），需要转换
```

### Q2: 颜色不对或花屏

**A:** 可能是像素格式转换问题
```cpp
// 检查源格式和目标格式
AVPixelFormat src_format = static_cast<AVPixelFormat>(frame->format);
AVPixelFormat dst_format = AV_PIX_FMT_BGR24;
```

### Q3: 解码延迟高

**A:** 启用低延迟模式
```cpp
codec_ctx_->flags |= AV_CODEC_FLAG_LOW_DELAY;
codec_ctx_->thread_count = 1;  // 减少线程数
```

---

## 📊 性能优化

### 1. 零拷贝（可选）

```cpp
// 当前实现使用 clone()，安全但有拷贝
// 如果需要极致性能，可以共享内存（但要注意生命周期）
cv::Mat mat(frame->height, frame->width, CV_8UC3, 
            out_data[0], out_linesize[0]);
// ⚠️ 需要自定义 deleter 释放 FFmpeg 内存
```

### 2. 批处理

```cpp
// 一次发送多个 NALU 到解码器
for (const auto& nalu : nalus) {
    decoder.decode(nalu.data(), nalu.size(), nalu.pts, callback);
}
```

### 3. 硬件加速

```cpp
// 使用 NVDEC/NVENC（需要额外配置）
AVCodec* codec = avcodec_find_decoder_by_name("h264_cuvid");
```

---

## 🚀 下一步

### 已完成的模块
- ✅ ZLMPuller（拉流器）
- ✅ FFmpegDecoder（解码器）
- ✅ FrameQueue（无锁队列）
- ✅ FrameData（帧数据）
- ✅ PipelineConfig（配置）
- ✅ 接口定义

### 待实现的模块
1. **OpenCVProcessor** - 图像处理器（下一个优先级）
2. **VideoPipeline** - 单个流水线
3. **VideoPipelineManager** - 流水线管理器

---

## 📖 参考资源

### FFmpeg
- [libavcodec API](https://ffmpeg.org/doxygen/trunk/group__lavc__decoding.html)
- [解码示例](https://github.com/FFmpeg/FFmpeg/blob/master/doc/examples/decode_video.c)

### H.264/H.265
- [H.264 标准文档](https://en.wikipedia.org/wiki/H.264/MPEG-4_AVC)
- [NALU 格式说明](https://yumichan.net/video-processing/video-compression/introduction-to-h264-nal-unit/)

### libswscale
- [图像转换文档](https://ffmpeg.org/doxygen/trunk/group__libswscale.html)

---

## ✅ 总结

✅ **FFmpegDecoder 已完成！**

- ✅ 完整的 H.264/H.265 解码功能
- ✅ YUV → BGR 格式转换
- ✅ 多线程解码支持
- ✅ 低延迟优化
- ✅ 详细的错误处理
- ✅ 完整的测试代码

🎯 **可以立即与 ZLMPuller 集成了！**

下一步建议：实现 **OpenCVProcessor**，完成图像增强处理。
