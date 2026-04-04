# ZLMPuller - ZLMediaKit HTTP-FLV 拉流器

## 📋 概述

`ZLMPuller` 是一个从 ZLMediaKit 服务器拉取 HTTP-FLV 流的组件，能够解析 FLV 格式并提取 H.264/H.265 NALU 数据包，供后续解码器使用。

**核心功能：**
- ✅ 通过 HTTP 长连接拉取 FLV 流
- ✅ 解析 FLV 容器格式（Header + Tags）
- ✅ 提取 H.264/H.265 NALU 数据
- ✅ 自动重连机制
- ✅ 关键帧缓存（快速恢复）
- ✅ 异步非阻塞 I/O

---

## 🏗️ 架构设计

### 类继承关系

```
IPuller (接口)
  ↑
ZLMPuller (实现)
```

### 工作流程

```
1. start(url, seq_cb, frame_cb)
   ↓
2. parseUrl() - 解析 URL
   ↓
3. connect() - TCP 连接
   ↓
4. sendHttpRequest() - 发送 HTTP GET
   ↓
5. readHttpResponse() - 读取 HTTP 响应头
   ↓
6. readFlvStream() - 开始读取 FLV 流
   ├─ readFlvHeader() - 读取 FLV 头 (9 bytes)
   ├─ readPreviousTagSize0() - 读取 PreviousTagSize0 (4 bytes)
   └─ async_read_tag() - 循环读取标签
       ├─ 读取 Tag Header (11 bytes)
       ├─ 读取 Tag Data (N bytes)
       ├─ handleFlvTag() - 处理标签
       │   └─ extractNalu() - 提取 NALU
       └─ 读取 PreviousTagSize (4 bytes)
```

---

## 🔧 API 参考

### 构造函数

```cpp
explicit ZLMPuller(boost::asio::io_context& io_ctx);
```

**参数：**
- `io_ctx`: Boost.Asio 的 io_context，用于异步网络操作

### start() - 启动拉流

```cpp
bool start(const std::string& url, 
           SequenceHeaderCallback seq_cb,
           FrameCallback frame_cb);
```

**参数：**
- `url`: HTTP-FLV 流地址，格式：`http://host:port/app/stream.live.flv`
- `seq_cb`: 序列头回调函数（SPS/PPS）
- `frame_cb`: 视频帧回调函数（NALU 数据）

**返回值：**
- `true`: 启动成功
- `false`: 启动失败（URL 解析错误或已在运行）

**示例：**
```cpp
auto puller = std::make_unique<ZLMPuller>(io_context);

puller->start(
    "http://127.0.0.1:80/live/cam1.live.flv",
    // 序列头回调
    [](int codec_id, const uint8_t* data, size_t size) {
        LOG_INFO("Received sequence header: codec={}, size={}", codec_id, size);
        // 初始化解码器
        decoder->init(codec_id, data, size);
    },
    // 视频帧回调
    [](const uint8_t* data, size_t size, int64_t pts) {
        LOG_DEBUG("Received frame: size={}, pts={}", size, pts);
        // 解码视频帧
        decoder->decode(data, size, pts);
    }
);
```

### stop() - 停止拉流

```cpp
void stop();
```

**作用：**
- 关闭 TCP 连接
- 停止所有异步操作
- 输出统计信息

### setReconnectParams() - 设置重连参数

```cpp
void setReconnectParams(int delay, int max_attempts);
```

**参数：**
- `delay`: 重连延迟（秒），默认 5 秒
- `max_attempts`: 最大重连次数，-1 表示无限重试

**示例：**
```cpp
// 每 3 秒重连一次，最多重试 10 次
puller->setReconnectParams(3, 10);

// 无限重试
puller->setReconnectParams(5, -1);
```

### isRunning() - 检查运行状态

```cpp
bool isRunning() const;
```

**返回值：**
- `true`: 正在拉流
- `false`: 已停止

---

## 📊 FLV 格式解析

### FLV 文件结构

```
┌─────────────────────────┐
│   FLV Header (9 bytes)  │
│  'F' 'L' 'V' version    │
│  flags data_offset      │
└─────────────────────────┘
┌─────────────────────────┐
│ PreviousTagSize0 (4B)   │ ← 始终为 0
└─────────────────────────┘
┌─────────────────────────┐
│   Tag1 Header (11B)     │
│  type size timestamp    │
│  ext_ts stream_id       │
└─────────────────────────┘
┌─────────────────────────┐
│   Tag1 Data (N bytes)   │
│  (视频/音频/脚本数据)     │
└─────────────────────────┘
┌─────────────────────────┐
│ PreviousTagSize1 (4B)   │ ← = 11 + N
└─────────────────────────┘
┌─────────────────────────┐
│   Tag2 Header (11B)     │
└─────────────────────────┘
...
```

### FLV Header (9 字节)

| Offset | Size | 字段 | 说明 |
|--------|------|------|------|
| 0-2 | 3 | Signature | `'F' 'L' 'V'` |
| 3 | 1 | Version | FLV 版本，通常为 `0x01` |
| 4 | 1 | Flags | 标志位：bit0=视频，bit2=音频 |
| 5-8 | 4 | DataOffset | 数据偏移量，通常为 `0x00000009` |

**示例：**
```
46 4C 56 01 05 00 00 00 09
 F  L  V  v1 flg offset
```

### FLV Tag Header (11 字节)

| Offset | Size | 字段 | 说明 |
|--------|------|------|------|
| 0 | 1 | TagType | 标签类型：8=音频，9=视频，18=脚本 |
| 1-3 | 3 | DataSize | 数据大小（大端序） |
| 4-6 | 3 | Timestamp | 时间戳（毫秒，大端序） |
| 7 | 1 | TimestampExtended | 时间戳扩展（高 8 位） |
| 8-10 | 3 | StreamID | 流 ID，通常为 0 |

**标签类型：**
- `8` (0x08): 音频标签
- `9` (0x09): 视频标签
- `18` (0x12): 脚本标签（onMetaData）

### PreviousTagSize (4 字节)

- 位于每个 Tag 之后
- 表示**前一个 Tag 的总大小**（Header + Data）
- PreviousTagSize0 始终为 0

---

## 🎥 视频标签解析

### FLV 视频标签格式

```
[FrameInfo(1B)][PacketType(1B)][CompositionTime(3B)][Data...]
```

| Offset | Size | 字段 | 说明 |
|--------|------|------|------|
| 0 | 1 | FrameInfo | 高 4 位=帧类型，低 4 位=编解码器 ID |
| 1 | 1 | PacketType | 包类型：0=序列头，1=NALU |
| 2-4 | 3 | CompositionTime | DTS 与 PTS 的差值 |
| 5+ | ... | Data | NALU 数据（AVCC 格式） |

### 编解码器 ID

| ID | 编解码器 |
|----|---------|
| 7 | H.264 / AVC |
| 12 | H.265 / HEVC |
| 13 | AV1 |
| 100 | VP8 |
| 101 | VP9 |

### PacketType

#### PacketType = 0（序列头）

包含 SPS/PPS（H.264）或 VPS/SPS/PPS（H.265），格式为 **AVCC**。

**用途：**
- 初始化解码器
- 网络波动后快速恢复

**回调：**
```cpp
seq_callback_(codec_id, data, size);
```

#### PacketType = 1（NALU 数据）

包含一个或多个 NALU，每个 NALU 前有 4 字节长度字段（大端序）。

**格式：**
```
[NALU1 Length(4B)][NALU1 Data][NALU2 Length(4B)][NALU2 Data]...
```

**回调：**
```cpp
for (each NALU) {
    frame_callback_(nalu_data, nalu_size, pts);
}
```

---

## 🔄 重连机制

### 触发条件

1. TCP 连接失败
2. HTTP 请求发送失败
3. HTTP 响应读取失败
4. FLV 头验证失败
5. 标签读取失败

### 重连流程

```
错误发生
  ↓
doReconnect()
  ↓
检查重连次数
  ├─ 达到最大次数 → 停止拉流
  └─ 未达最大次数
      ↓
  reset() - 重置状态
      ↓
  sleep(delay) - 延迟
      ↓
  connect() - 重新连接
```

### 状态重置

`reset()` 函数会重置以下状态：
- `has_flv_header_ = false`
- `expected_tag_size_ = 0`
- `current_tag_data_.clear()`
- `bytes_received_ = 0`
- `tags_processed_ = 0`
- `frames_delivered_ = 0`

**注意：** 关键帧缓存（`last_sps_pps_h264_` / `last_sps_pps_h265_`）**不会**被清除，用于快速恢复。

---

## 💾 关键帧缓存

### 缓存内容

- **H.264**: SPS + PPS（AVCC 格式）
- **H.265**: VPS + SPS + PPS（AVCC 格式）

### 缓存时机

每次收到 `PacketType = 0`（序列头）时更新缓存：

```cpp
void cacheKeyframe(int codec_id, const uint8_t* data, size_t size) {
    if (codec_id == 7) {
        last_sps_pps_h264_.assign(data, data + size);
    } else if (codec_id == 12) {
        last_sps_pps_h265_.assign(data, data + size);
    }
}
```

### 使用场景

1. **解码器初始化** - 首次收到序列头时
2. **网络恢复** - 重连后可立即使用缓存的序列头
3. **解码器重置** - 解码器出错时可重新初始化

### 获取缓存

```cpp
const std::vector<uint8_t>& getLastSpsPpsH264() const;
const std::vector<uint8_t>& getLastSpsPpsH265() const;
```

---

## 📈 统计信息

### 统计指标

| 指标 | 类型 | 说明 |
|------|------|------|
| `bytes_received_` | atomic<uint64_t> | 接收的总字节数 |
| `tags_processed_` | atomic<uint64_t> | 处理的标签数 |
| `frames_delivered_` | atomic<uint64_t> | 交付的帧数 |

### 查看统计

停止拉流时会自动输出：

```cpp
LOG_MAIN_INFO_AT("ZLMPuller stopped. Stats: bytes={}, tags={}, frames={}",
                bytes_received_.load(),
                tags_processed_.load(),
                frames_delivered_.load());
```

**示例输出：**
```
[info] ZLMPuller stopped. Stats: bytes=1234567, tags=8901, frames=5678
```

---

## 🛠️ 内部函数详解

### 1. parseUrl() - URL 解析

```cpp
bool parseUrl(const std::string& url);
```

**支持的格式：**
- `http://host/app/stream.live.flv`
- `http://host:port/app/stream.live.flv`

**正则表达式：**
```cpp
R"(http://([^:/]+)(?::(\d+))?(/.+\.live\.flv))"
```

**提取内容：**
- `host_`: 主机名
- `port_`: 端口号（默认 80）
- `path_`: 路径（移除开头的 `/`）

### 2. connect() - TCP 连接

```cpp
void connect();
```

**步骤：**
1. 创建 DNS resolver
2. 解析主机名
3. 异步连接到第一个可用端点
4. 连接成功后调用 `sendHttpRequest()`

**错误处理：**
- 连接失败 → `doReconnect()`

### 3. sendHttpRequest() - 发送 HTTP 请求

```cpp
void sendHttpRequest();
```

**构建的请求：**
```http
GET /live/cam1.live.flv HTTP/1.1
Host: 127.0.0.1
User-Agent: ZLMPuller/1.0
Accept: */*
Connection: keep-alive
```

**关键点：**
- 使用 `shared_ptr` 管理 request 生命周期
- 设置 `keep_alive(true)` 保持长连接

### 4. readHttpResponse() - 读取 HTTP 响应

```cpp
void readHttpResponse();
```

**步骤：**
1. 使用 `async_read_until` 读取到 `\r\n\r\n`
2. 检查响应是否为空
3. 使用 `beast::buffers_to_string` 安全转换
4. 消耗掉 HTTP 响应头
5. 调用 `readFlvStream()`

**安全检查：**
```cpp
if (bytes_transferred == 0) {
    LOG_MAIN_ERROR_AT("HTTP response is empty");
    doReconnect();
    return;
}
```

### 5. readFlvStream() - FLV 流入口

```cpp
void readFlvStream();
```

**逻辑：**
```cpp
if (!has_flv_header_) {
    readFlvHeader();  // 首次读取 FLV 头
}
else {
    async_read_tag();  // 继续读取标签
}
```

### 6. readFlvHeader() - 读取 FLV 头

```cpp
void readFlvHeader();
```

**步骤：**
1. 异步读取 9 字节
2. 验证签名 `'F' 'L' 'V'`
3. 记录版本和标志位
4. 设置 `has_flv_header_ = true`
5. 调用 `readPreviousTagSize0()`

**验证：**
```cpp
if (flv_header_buffer_[0] != 'F' || 
    flv_header_buffer_[1] != 'L' ||
    flv_header_buffer_[2] != 'V') {
    LOG_MAIN_ERROR_AT("Invalid FLV signature");
    doReconnect();
    return;
}
```

### 7. readPreviousTagSize0() - 读取 PreviousTagSize0

```cpp
void readPreviousTagSize0();
```

**步骤：**
1. 异步读取 4 字节
2. 解析为大端序整数
3. 验证值为 0（警告如果不为 0）
4. 调用 `async_read_tag()`

**解析：**
```cpp
uint32_t prev_size = (static_cast<uint32_t>(prev_tag_size_buffer_[0]) << 24) |
                    (static_cast<uint32_t>(prev_tag_size_buffer_[1]) << 16) |
                    (static_cast<uint32_t>(prev_tag_size_buffer_[2]) << 8) |
                    static_cast<uint32_t>(prev_tag_size_buffer_[3]);
```

### 8. async_read_tag() - 异步读取标签

```cpp
void async_read_tag();
```

**流程：**
```
1. 读取 Tag Header (11 bytes)
   ↓
2. parseFlvTagHeader() - 解析标签头
   ↓
3. 计算标签体大小
   ↓
4. 读取 Tag Data (N bytes)
   ↓
5. handleFlvTag() - 处理标签
   ↓
6. 读取 PreviousTagSize (4 bytes)
   ↓
7. async_read_tag() - 递归读取下一个标签
```

### 9. parseFlvTagHeader() - 解析标签头

```cpp
int parseFlvTagHeader(const uint8_t* data, size_t size);
```

**返回：**
- 标签类型（8/9/18）
- 0 表示无效

**调试日志：**
```cpp
LOG_MAIN_DEBUG_AT("{}", hex_str);  // 十六进制输出标签头
LOG_MAIN_DEBUG_AT("FLV Tag: type={}, timestamp={}ms, size={}",
                 tag_type, timestamp, expected_tag_size_);
```

### 10. handleFlvTag() - 处理标签

```cpp
void handleFlvTag(const uint8_t* data, size_t size);
```

**分支处理：**
- `tag_type == 9` (视频) → `extractNalu()`
- `tag_type == 8` (音频) → 记录日志（暂不处理）
- `tag_type == 18` (脚本) → 记录日志

### 11. extractNalu() - 提取 NALU

```cpp
void extractNalu(const uint8_t* data, size_t size, int64_t pts);
```

**处理流程：**
```
1. 检查最小大小 (>= 5 bytes)
   ↓
2. 提取 FrameInfo 和 PacketType
   ↓
3. 检查编解码器 (只支持 H.264/H.265)
   ↓
4. 跳过 5 字节标签头
   ↓
5. 根据 PacketType 处理：
   ├─ PacketType == 0 (序列头)
   │   ├─ 缓存关键帧
   │   ├─ 调用 seq_callback_
   │   └─ frames_delivered_++
   └─ PacketType == 1 (NALU)
       ├─ 遍历所有 NALU
       ├─ 提取每个 NALU（4 字节长度 + 数据）
       ├─ 调用 frame_callback_
       └─ frames_delivered_++
```

**NALU 提取：**
```cpp
while (offset + 4 <= nalu_size) {
    uint32_t nalu_len = (nalu_data[offset] << 24) |
                       (nalu_data[offset + 1] << 16) |
                       (nalu_data[offset + 2] << 8) |
                       nalu_data[offset + 3];
    
    offset += 4;
    
    if (offset + nalu_len > nalu_size) break;
    
    callback_(nalu_data + offset, nalu_len, pts);
    frames_delivered_++;
    
    offset += nalu_len;
}
```

---

## 🧪 测试示例

### 基本用法

```cpp
#include "video_pipeline/puller/zlm_puller.h"
#include <boost/asio.hpp>
#include <iostream>

int main() {
    boost::asio::io_context io_context;
    
    auto puller = std::make_unique<ZLMPuller>(io_context);
    
    // 设置重连参数
    puller->setReconnectParams(5, -1);  // 无限重试
    
    // 启动拉流
    bool success = puller->start(
        "http://127.0.0.1:80/live/cam1.live.flv",
        // 序列头回调
        [](int codec_id, const uint8_t* data, size_t size) {
            std::cout << "Sequence header: codec=" << codec_id 
                      << ", size=" << size << std::endl;
        },
        // 视频帧回调
        [](const uint8_t* data, size_t size, int64_t pts) {
            std::cout << "Frame: size=" << size 
                      << ", pts=" << pts << std::endl;
        }
    );
    
    if (!success) {
        std::cerr << "Failed to start puller" << std::endl;
        return 1;
    }
    
    // 运行 io_context
    io_context.run();
    
    return 0;
}
```

### 与解码器集成

```cpp
class VideoDecoder {
public:
    void init(int codec_id, const uint8_t* sps_pps, size_t size) {
        // 初始化解码器
        if (codec_id == 7) {
            // H.264
            avcodec_parameters_from_extradata(sps_pps, size);
        } else if (codec_id == 12) {
            // H.265
            avcodec_parameters_from_extradata(sps_pps, size);
        }
    }
    
    void decode(const uint8_t* nalu, size_t size, int64_t pts) {
        // 解码 NALU
        avcodec_send_packet(nalu, size);
        avcodec_receive_frame(frame);
    }
};

int main() {
    boost::asio::io_context io_context;
    auto decoder = std::make_unique<VideoDecoder>();
    auto puller = std::make_unique<ZLMPuller>(io_context);
    
    puller->start(
        "http://127.0.0.1:80/live/cam1.live.flv",
        [decoder](int codec_id, const uint8_t* data, size_t size) {
            decoder->init(codec_id, data, size);
        },
        [decoder](const uint8_t* data, size_t size, int64_t pts) {
            decoder->decode(data, size, pts);
        }
    );
    
    io_context.run();
    return 0;
}
```

---

## ⚠️ 注意事项

### 1. 线程安全

- 所有回调在 **io_context 线程**中执行
- 不要在回调中执行耗时操作
- 如需跨线程通信，使用 `post()` 或 `dispatch()`

### 2. 内存管理

- `ZLMPuller` 析构时会自动调用 `stop()`
- 确保 `io_context` 的生命周期长于 `ZLMPuller`
- 使用 `std::unique_ptr` 或 `std::shared_ptr` 管理实例

### 3. 网络异常

- 所有网络错误都会触发重连
- 重连会重置统计计数器
- 关键帧缓存会保留

### 4. 性能优化

- 使用异步 I/O，避免阻塞
- FLV 标签逐个处理，无需缓冲整个流
- 关键帧缓存避免重复传输 SPS/PPS

### 5. 支持的编解码器

当前支持：
- ✅ H.264 / AVC (codec_id = 7)
- ✅ H.265 / HEVC (codec_id = 12)

待支持：
- ⏳ AV1 (codec_id = 13)
- ⏳ VP8 (codec_id = 100)
- ⏳ VP9 (codec_id = 101)

---

## 📚 相关文档

- [ZLM_PULLER_BUFFER_FIX.md](./ZLM_PULLER_BUFFER_FIX.md) - 缓冲区迭代器越界修复
- [ZLM_PULLER_HTTP_DEBUG.md](./ZLM_PULLER_HTTP_DEBUG.md) - HTTP 响应调试指南
- [ZLM_PULLER_REFACTOR_EXTRACT_FUNCTIONS.md](./ZLM_PULLER_REFACTOR_EXTRACT_FUNCTIONS.md) - 代码重构说明
- [FLV_PREVIOUS_TAG_SIZE_FIX.md](./FLV_PREVIOUS_TAG_SIZE_FIX.md) - PreviousTagSize0 修复

---

## 📝 总结

`ZLMPuller` 是一个高效、可靠的 HTTP-FLV 拉流器，具有以下特点：

✅ **完整的 FLV 解析** - 严格遵循 Adobe FLV 规范  
✅ **异步非阻塞** - 基于 Boost.Asio，高性能  
✅ **自动重连** - 网络波动自动恢复  
✅ **关键帧缓存** - 快速初始化和恢复  
✅ **清晰的架构** - 函数提取，易于维护  
✅ **详细的日志** - 便于调试和问题排查  

适用于视频监控、直播拉流等场景。
