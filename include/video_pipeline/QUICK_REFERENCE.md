# 视频流水线快速参考

## 📚 核心 API

### FrameQueue（无锁 SPSC 队列）

```cpp
#include "video_pipeline/frame_queue.h"

// 创建队列
FrameQueue<int> queue(64);  // 容量 64

// 推入数据（非阻塞）
bool success = queue.push(item);

// 弹出数据（带超时）
auto opt = queue.pop(std::chrono::milliseconds(100));
if (opt.has_value()) {
    process(opt.value());
}

// 尝试弹出（不等待）
auto opt = queue.try_pop();

// 状态查询
queue.empty();      // 是否为空
queue.full();       // 是否已满
queue.size();       // 当前大小

// 控制操作
queue.shutdown();   // 关闭队列
queue.clear();      // 清空队列
```

---

### FrameData（帧数据）

```cpp
#include "video_pipeline/frame_data.h"

// 创建帧数据
cv::Mat frame = cv::imread("image.jpg");
FrameData data(channel_id, timestamp_us, std::move(frame));

// 访问属性
int width = data.width;
int height = data.height;
int64_t pts = data.pts;

// 检查有效性
if (data.isValid()) {
    processData(data);
}

// 清空数据
data.clear();
```

---

### PipelineConfig（配置）

```cpp
#include "video_pipeline/pipeline_config.h"

PipelineConfig config;
config.stream_url = "rtsp://192.168.1.100/live";
config.channel_id = 1;
config.decoder_threads = 2;
config.enable_preprocess = true;
config.filters = {"gaussian_blur", "histogram_eq"};
config.algorithm_type = "yolo_v5";
config.model_path = "./models/yolov5.onnx";

// 验证配置
if (config.isValid()) {
    createPipeline(config);
}
```

---

## 🎯 接口定义

### IPuller（拉流器）

```cpp
class IPuller {
public:
    using FrameCallback = std::function<void(const uint8_t* data, int size, int64_t pts)>;
    
    virtual bool start(const std::string& url, FrameCallback cb) = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
};
```

**使用示例：**
```cpp
auto puller = std::make_unique<ZLMPuller>(io_context);
puller->start(url, 
    [](int codec_id, const uint8_t* data, int size) {
        // 处理序列头
    },
    [](const uint8_t* data, int size, int64_t pts) {
        // 处理拉流数据
    });
```

---

### IDecoder（解码器）

```cpp
class IDecoder {
public:
    using FrameCallback = std::function<void(cv::Mat&& frame, int64_t pts)>;
    
    virtual bool open(const uint8_t* extradata, int size, int codec_id) = 0;
    virtual void decode(const uint8_t* packet, int size, int64_t pts, FrameCallback cb) = 0;
    virtual void close() = 0;
};
```

**使用示例：**
```cpp
auto decoder = std::make_unique<FFmpegDecoder>();
decoder->open(extradata, size, AV_CODEC_ID_H264);
decoder->decode(packet, size, pts, [](cv::Mat&& frame, int64_t pts) {
    // 处理解码后的帧
});
```

---

### IProcessor（处理器）

```cpp
class IProcessor {
public:
    virtual cv::Mat process(cv::Mat&& input) = 0;
};
```

**使用示例：**
```cpp
auto processor = std::make_unique<OpenCVProcessor>(filters);
cv::Mat processed = processor->process(std::move(input_frame));
```

---

### IAlgorithm（算法）

```cpp
class IAlgorithm {
public:
    virtual void infer(const cv::Mat& frame, int64_t timestamp_us) = 0;
};
```

**使用示例：**
```cpp
auto algorithm = std::make_unique<YoloAlgorithm>(model_path);
algorithm->infer(frame, timestamp_us);
```

---

## 🔧 典型使用场景

### 场景 1：单路视频处理

```cpp
// 1. 创建配置
PipelineConfig config;
config.stream_url = "rtsp://192.168.1.100/live";
config.algorithm_type = "yolo_v5";

// 2. 创建流水线
VideoPipeline pipeline(config);
pipeline.start();

// 3. 停止并清理
pipeline.stop();
```

### 场景 2：多路视频处理

```cpp
// 使用管理器
auto& manager = VideoPipelineManager::GetInstance();

// 创建多路流水线
std::vector<PipelineConfig> configs = {
    config1, config2, config3
};

auto channel_ids = manager.createPipelines(configs);

// 查询状态
size_t count = manager.getPipelineCount();
auto ids = manager.getAllChannelIds();

// 销毁某一路
manager.destroyPipeline(channel_ids[0]);

// 全部销毁
manager.destroyAllPipelines();
```

### 场景 3：自定义滤镜

```cpp
class MyCustomFilter : public IProcessor {
public:
    cv::Mat process(cv::Mat&& input) override {
        cv::Mat output;
        
        // 自定义处理逻辑
        cv::cvtColor(input, output, cv::COLOR_BGR2GRAY);
        cv::threshold(output, output, 127, 255, cv::THRESH_BINARY);
        
        return output;
    }
};

// 使用
auto filter = std::make_unique<MyCustomFilter>();
```

---

## ⚠️ 注意事项

### 1. 队列容量设置

```cpp
// 推荐配置
raw_queue_size = 64;      // 原始包队列：较大
decoded_queue_size = 16;  // 解码帧队列：中等
processed_queue_size = 16; // 处理帧队列：中等

// 原因：
// - 原始包较小，可以大一些
// - 解码后的帧数据较大，不宜太大
// - 避免内存占用过高
```

### 2. 背压处理

```cpp
// 当队列满时的策略
if (!queue.push(item)) {
    // 方案 1：丢弃（适合实时性要求高的场景）
    dropped_frames_++;
    
    // 方案 2：降帧率
    if (dropped_frames_ > 30) {
        LOG_WARN("Too many drops, reducing FPS");
    }
    
    // 方案 3：记录日志用于调试
    LOG_DEBUG("Queue full, dropping frame");
}
```

### 3. 优雅关闭

```cpp
void stop() {
    // 1. 设置停止标志
    running_ = false;
    
    // 2. 关闭所有队列（唤醒等待的消费者）
    raw_queue_.shutdown();
    decoded_queue_.shutdown();
    processed_queue_.shutdown();
    
    // 3. 停止拉流器
    puller_->stop();
    
    // 4. 等待所有线程结束
    pull_thread_.join();
    decode_thread_.join();
    process_thread_.join();
    algorithm_thread_.join();
}
```

---

## 📊 性能优化技巧

### 1. 内存池

```cpp
// 复用 cv::Mat，避免频繁分配
class FramePool {
public:
    cv::Mat acquire() {
        if (pool_.empty()) {
            return cv::Mat(480, 640, CV_8UC3);
        }
        auto mat = std::move(pool_.back());
        pool_.pop_back();
        return mat;
    }
    
    void release(cv::Mat mat) {
        mat.release();
        pool_.push_back(std::move(mat));
    }
    
private:
    std::vector<cv::Mat> pool_;
};
```

### 2. 零拷贝

```cpp
// 使用移动语义
void processFrame() {
    auto opt = queue.pop();
    if (opt.has_value()) {
        // ✅ 好：移动语义
        processor->process(std::move(opt->frame));
        
        // ❌ 不好：不必要的拷贝
        // processor->process(opt->frame.clone());
    }
}
```

### 3. 批处理

```cpp
// 算法模块批量处理
void algorithmThread() {
    std::vector<FrameData> batch;
    batch.reserve(4);
    
    while (running) {
        auto opt = queue.pop(std::chrono::milliseconds(10));
        if (opt.has_value()) {
            batch.push_back(std::move(opt.value()));
            
            // 达到批次大小或超时，批量处理
            if (batch.size() >= 4) {
                algorithm->inferBatch(batch);
                batch.clear();
            }
        }
    }
}
```

---

## 🐛 常见问题

### Q1: 队列应该设置多大？

**A:** 根据实际场景：
- **低延迟场景**：小队列（8-16）
- **高吞吐场景**：大队列（64-128）
- **网络不稳定**：适当增大队列缓冲

### Q2: 如何处理丢帧？

**A:** 三种策略：
1. **直接丢弃** - 简单高效
2. **降帧率** - 通知拉流端降低 FPS
3. **动态调整** - 根据队列长度自适应

### Q3: 如何保证线程安全？

**A:** 
- ✅ SPSC 队列本身是线程安全的
- ✅ 每个线程只访问自己的队列
- ❌ 不要在多个生产者之间共享队列

---

## 📖 相关文档

- [实现总结](./IMPLEMENTATION_SUMMARY.md) - 详细的架构设计
- [Boost Lockfree 文档](https://www.boost.org/doc/libs/release/doc/html/lockfree.html)
- [FFmpeg 解码示例](https://github.com/FFmpeg/FFmpeg/blob/master/doc/examples/decode_video.c)
- [OpenCV 图像处理](https://docs.opencv.org/master/d2/de8/group__core__array.html)
