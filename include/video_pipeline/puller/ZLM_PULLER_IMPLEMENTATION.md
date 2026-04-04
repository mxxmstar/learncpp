# ZLMPuller 实现总结

## ✅ 已完成的工作

### 📁 创建的文件

1. **include/video_pipeline/puller/zlm_puller.h** - ZLMPuller 头文件
2. **src/video_pipeline/puller/zlm_puller.cpp** - ZLMPuller 实现文件
3. **test/video_pipeline/test_zlm_puller.cpp** - ZLMPuller 测试文件

---

## 🎯 ZLMPuller 功能说明

### 核心功能

ZLMPuller 是一个专门用于从 ZLMediaKit 服务器拉取 HTTP-FLV 流的组件，主要功能包括：

1. **HTTP-FLV 流拉取** - 通过 HTTP 协议接收 FLV 格式的直播流
2. **FLV 解复用** - 解析 FLV 容器格式，分离视频和音频数据
3. **H.264/H.265 提取** - 从 FLV 视频标签中提取 H.264/H.265 NALU 数据包
4. **自动重连** - 网络断开时自动尝试重连
5. **统计信息** - 实时统计接收的字节数、标签数、帧数

---

## 📋 技术架构

### 类结构

```cpp
class ZLMPuller : public IPuller {
    // 继承自 IPuller 接口
    // 使用 Boost.Beast 进行 HTTP 通信
    // 异步非阻塞 I/O
};
```

### 依赖关系

```
ZLMPuller
├── IPuller (接口)
├── Boost.Asio (网络 I/O)
├── Boost.Beast (HTTP 协议)
└── FFmpeg (间接依赖，用于后续解码)
```

---

## 🔧 核心方法详解

### 1. start() - 启动拉流

```cpp
bool start(const std::string& url, FrameCallback cb);
```

**参数：**
- `url` - HTTP-FLV 流地址（格式：`http://host:port/app/stream.flv`）
- `cb` - 数据回调函数，原型：`void(const uint8_t* data, int size, int64_t pts)`

**流程：**
1. 解析 URL，提取 host、port、path
2. 设置回调函数
3. 异步连接服务器
4. 发送 HTTP GET 请求
5. 开始读取 FLV 流

**示例：**
```cpp
auto puller = std::make_unique<ZLMPuller>(io_ctx);
puller->start("http://127.0.0.1:8080/live/test.flv",
    [](int codec_id, const uint8_t* data, int size) {
        // 处理序列头（SPS/PPS）
    },
    [](const uint8_t* data, int size, int64_t pts) {
        // 处理 NALU 数据
    });
```

---

### 2. connect() - 连接服务器

```cpp
void connect();
```

**流程：**
1. 使用 `tcp::resolver` 解析主机名
2. 异步连接到服务器
3. 连接失败时触发重连逻辑

**错误处理：**
- DNS 解析失败 → 重连
- 连接被拒绝 → 重连
- 超时 → 重连

---

### 3. sendHttpRequest() - 发送 HTTP 请求

```cpp
void sendHttpRequest();
```

**构建的 HTTP 请求：**
```http
GET /live/test.flv HTTP/1.1
Host: 127.0.0.1
User-Agent: ZLMPuller/1.0
Accept: */*
Connection: keep-alive
```

---

### 4. readFlvStream() - 读取 FLV 流

```cpp
void readFlvStream();
```

**FLV 流格式：**
```
[FLV Header (9 bytes)]
[PreviousTagSize0 (4 bytes)]
[Tag1 Header (11 bytes)]
[Tag1 Data]
[PreviousTagSize1 (4 bytes)]
[Tag2 Header (11 bytes)]
[Tag2 Data]
...
```

**读取步骤：**
1. 读取并验证 FLV 头（9 字节）
   - 签名：'F''L''V'
   - 版本：通常是 1
   - 标志：0x05（有视频和音频）
   
2. 循环读取标签：
   a. 读取标签头（11 字节）
   b. 解析标签类型和时间戳
   c. 读取标签体数据
   d. 读取 PreviousTagSize（4 字节）

---

### 5. handleFlvTag() - 处理 FLV 标签

```cpp
void handleFlvTag(const uint8_t* data, size_t size);
```

**标签类型：**
- **8 (0x08)** - 音频标签
- **9 (0x09)** - 视频标签
- **18 (0x12)** - 脚本数据（onMetaData 等）

**处理逻辑：**
```cpp
switch (tag_type) {
    case VIDEO:
        extractNalu(data, size, pts);  // 提取 NALU
        break;
    case AUDIO:
        // 可以传递给音频解码器
        break;
    case SCRIPT:
        // 解析元数据
        break;
}
```

---

### 6. extractNalu() - 提取 H.264/H.265 NALU

```cpp
void extractNalu(const uint8_t* data, size_t size, int64_t pts);
```

**FLV 视频标签格式：**
```
[FrameType(4bits)][CodecID(4bits)]  // 1 byte
[PacketType(8bits)]                  // 1 byte
[CompositionTime(24bits)]            // 3 bytes
[Data...]                            // NALU 数据
```

**编解码器 ID：**
- **7** - H.264
- **12** - H.265

**PacketType：**
- **0** - 序列头（SPS/PPS）
- **1** - NALU 数据

**NALU 格式（Annex B）：**
```
[NALU Length (4 bytes, big-endian)]
[NALU Data...]
[NALU Length (4 bytes)]
[NALU Data...]
...
```

**提取过程：**
1. 跳过 5 字节的 FLV 视频标签头
2. 遍历所有 NALU：
   - 读取 4 字节的 NALU 长度
   - 提取 NALU 数据
   - 调用回调函数传递 NALU

---

### 7. doReconnect() - 重连逻辑

```cpp
void doReconnect();
```

**重连策略：**
```cpp
if (max_reconnect_attempts >= 0 && 
    reconnect_count >= max_reconnect_attempts) {
    // 达到最大重试次数，停止
    running_ = false;
    return;
}

reconnect_count++;
sleep(reconnect_delay);  // 延迟等待
connect();  // 重新连接
```

**配置参数：**
- `reconnect_delay` - 重连延迟（秒），默认 3 秒
- `max_reconnect_attempts` - 最大重试次数（-1=无限重试）

---

## 📊 数据结构

### FLV 头（9 字节）

```
Offset  Size  Description
------  ----  -----------
0       3     Signature: 'F''L''V'
3       1     Version: 0x01
4       1     Flags: 0x05 (video + audio)
5       4     DataOffset: 9 (header size)
```

### FLV 标签头（11 字节）

```
Offset  Size  Description
------  ----  -----------
0       1     Tag Type (8=audio, 9=video, 18=script)
1       3     DataSize (big-endian)
4       3     Timestamp (lower 24 bits)
7       1     TimestampExtended (upper 8 bits)
8       3     StreamID (always 0)
```

### 时间戳计算

```cpp
uint32_t timestamp = 
    (data[10] << 16) |  // 高 8 位
    (data[9] << 8) |    // 中 8 位
    data[8];            // 低 8 位
```

---

## 🔍 使用示例

### 基本用法

```cpp
#include "video_pipeline/puller/zlm_puller.h"

// 创建 io_context
boost::asio::io_context io_ctx;

// 创建拉流器
auto puller = std::make_unique<ZLMPuller>(io_ctx);

// 设置回调
puller->start("http://127.0.0.1:8080/live/test.flv",
    [](int codec_id, const uint8_t* data, int size) {
        std::cout << "Sequence Header: Codec=" << codec_id 
                  << ", Size=" << size << " bytes" << std::endl;
    },
    [](const uint8_t* data, int size, int64_t pts) {
        std::cout << "Received NALU: " << size << " bytes @ " 
                  << pts << "ms" << std::endl;
        
        // 可以将数据推入队列或直接传递给解码器
    });

// 运行 io_context
io_ctx.run();
```

### 配置重连参数

```cpp
puller->setReconnectParams(
    5,   // 重连延迟 5 秒
    10   // 最多重试 10 次
);
```

### 与 FrameQueue 配合使用

```cpp
auto queue = std::make_shared<RawPacketQueue>(64);

puller->start(url, 
    [queue](int codec_id, const uint8_t* data, int size) {
        // 可选：处理序列头
        LOG_INFO("Sequence header received, codec={}", codec_id);
    },
    [queue](const uint8_t* data, int size, int64_t pts) {
        RawPacketData packet(0, pts, data, size);
        if (!queue->push(std::move(packet))) {
            // 队列已满，丢弃
            LOG_WARN("Queue full, dropping frame");
        }
    });
```

---

## ⚠️ 注意事项

### 1. URL 格式

必须是有效的 HTTP-FLV 格式：
```
✅ http://127.0.0.1:8080/live/test.flv
✅ http://example.com:1935/app/stream.flv
❌ rtsp://127.0.0.1/live/test  (不支持 RTSP)
❌ rtmp://127.0.0.1/live/test  (不支持 RTMP)
```

### 2. 线程安全

- ✅ `start()` 和 `stop()` 应在主线程调用
- ✅ 回调函数在 IO 线程中执行
- ✅ 回调中应避免耗时操作
- ✅ 建议使用队列缓冲数据

### 3. 内存管理

```cpp
// ❌ 不好的做法：在回调中拷贝大量数据
puller->start(url, 
    [](int codec_id, const uint8_t* data, int size) {},
    [](const uint8_t* data, int size, int64_t pts) {
        std::vector<uint8_t> copy(data, data + size);  // 不必要的拷贝
        process(copy);
    });

// ✅ 好的做法：直接传递或移动到队列
puller->start(url, 
    [](int codec_id, const uint8_t* data, int size) {},
    [queue](const uint8_t* data, int size, int64_t pts) {
        RawPacketData packet(0, pts, data, size);
        queue->push(std::move(packet));  // 移动语义
    });
```

### 4. 异常处理

```cpp
try {
    puller->start(url, 
        [](int codec_id, const uint8_t* data, int size) {},
        [](const uint8_t* data, int size, int64_t pts) {});
    io_ctx.run();
}
catch (const std::exception& e) {
    LOG_MAIN_ERROR_AT("Puller error: {}", e.what());
}
```

---

## 🐛 常见问题

### Q1: 连接失败怎么办？

**A:** 检查以下几点：
1. ZLMediaKit 是否正常运行
2. 流地址是否正确
3. 防火墙是否阻止连接
4. 查看日志中的详细错误信息

### Q2: 收不到数据？

**A:** 可能的原因：
1. 流不存在或未推流到 ZLMediaKit
2. URL 路径错误
3. 网络问题
4. 检查 FLV 头验证是否通过

### Q3: 如何调试？

**A:** 启用详细日志：
```cpp
// 在配置中设置日志级别为 debug
config.log_level = 0;  // 0=debug, 1=info, 2=warn, 3=error
```

查看关键日志：
```
[Info] Parsed URL: host=127.0.0.1, port=8080, path=live/test.flv
[Info] Connected to 127.0.0.1:8080
[Info] Sent HTTP request (XXX bytes)
[Info] FLV header validated
[Debug] FLV Tag: type=9, timestamp=XXXms, size=XXX
[Info] Video sequence header (SPS/PPS): XX bytes @ XXXms
```

---

## 📈 性能优化

### 1. 缓冲区大小

```cpp
// FLV 标签通常不会太大
std::vector<uint8_t> current_tag_data_;  // 动态分配
// 典型大小：
// - SPS/PPS: ~50 bytes
// - I 帧：~5000 bytes
// - P 帧：~1000 bytes
```

### 2. 零拷贝

```cpp
// 直接在原始数据上操作，避免额外拷贝
callback_(nalu_data + offset, nalu_len, pts);
```

### 3. 异步 I/O

```cpp
// 所有网络操作都是异步的
boost::asio::async_read(..., callback);
boost::asio::async_write(..., callback);
```

### 4. 重连退避

```cpp
// 可以使用指数退避策略
reconnect_delay_ = std::min(30, reconnect_delay_ * 2);
```

---

## 🚀 下一步

### 已完成的模块
- ✅ ZLMPuller（拉流器）
- ✅ FrameQueue（无锁队列）
- ✅ FrameData（帧数据）
- ✅ PipelineConfig（配置）
- ✅ 接口定义（IPuller, IDecoder, IProcessor, IAlgorithm）

### 待实现的模块
1. **FFmpegDecoder** - FFmpeg 解码器（下一个优先级）
2. **OpenCVProcessor** - OpenCV 图像处理器
3. **VideoPipeline** - 单个流水线
4. **VideoPipelineManager** - 流水线管理器

---

## 📖 参考资源

### FLV 格式规范
- [FLV Specification](https://www.adobe.com/content/dam/acom/en/devnet/flv/video_file_format_spec_v10_1.pdf)

### HTTP-FLV 协议
- [RFC 7230 - HTTP/1.1](https://tools.ietf.org/html/rfc7230)

### H.264/H.265
- [H.264 Overview](https://en.wikipedia.org/wiki/H.264/MPEG-4_AVC)
- [H.265 Overview](https://en.wikipedia.org/wiki/High_Efficiency_Video_Coding)

### Boost.Beast
- [Boost.Beast Documentation](https://www.boost.org/doc/libs/release/libs/beast/)

---

## ✅ 总结

✅ **ZLMPuller 已完成！**

- ✅ 完整的 HTTP-FLV 拉流功能
- ✅ FLV 解复用和 H.264/H.265 提取
- ✅ 自动重连和错误恢复
- ✅ 详细的统计信息
- ✅ 异步非阻塞 I/O
- ✅ 完善的测试代码

🎯 **可以开始集成到流水线了！**

下一步建议：实现 **FFmpegDecoder**，将 NALU 解码为 OpenCV Mat。
