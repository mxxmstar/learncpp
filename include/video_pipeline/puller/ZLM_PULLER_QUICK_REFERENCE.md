# ZLMPuller 快速参考

## 📚 API 速查

### 创建和使用

```cpp
#include "video_pipeline/puller/zlm_puller.h"

// 1. 创建 io_context
boost::asio::io_context io_ctx;

// 2. 创建拉流器
auto puller = std::make_unique<ZLMPuller>(io_ctx);

// 3. 启动拉流
bool success = puller->start(
    "http://127.0.0.1:8080/live/test.flv",
    [](const uint8_t* data, int size, int64_t pts) {
        // 回调函数：接收 NALU 数据
        std::cout << "Received " << size << " bytes @ " 
                  << pts << "ms" << std::endl;
    }
);

// 4. 运行 io_context
io_ctx.run();

// 5. 停止拉流
puller->stop();
```

---

### URL 格式

```cpp
// ✅ 正确的格式
"http://host:port/app/stream.flv"

// 示例
"http://127.0.0.1:8080/live/camera1.flv"
"http://192.168.1.100:1935/rtmp/stream.flv"

// ❌ 错误的格式（不支持）
"rtsp://127.0.0.1/live/stream"   // RTSP
"rtmp://127.0.0.1/live/stream"   // RTMP
```

---

### 配置重连

```cpp
// 设置重连参数
puller->setReconnectParams(
    5,   // 重连延迟：5 秒
    -1   // 无限重试（或设置为具体次数，如 10）
);

// 典型配置
puller->setReconnectParams(3, -1);     // 生产环境
puller->setReconnectParams(1, 5);      // 测试环境
```

---

## 🔧 回调函数

### 函数签名

```cpp
using FrameCallback = std::function<void(
    const uint8_t* data,   // NALU 数据指针
    int size,              // 数据大小
    int64_t pts            // 时间戳（毫秒）
)>;
```

### 使用示例

#### 示例 1：直接打印信息

```cpp
puller->start(url, [](const uint8_t* data, int size, int64_t pts) {
    std::cout << "Frame: " << size << " bytes @ " 
              << pts << "ms" << std::endl;
});
```

#### 示例 2：推入队列

```cpp
auto queue = std::make_shared<RawPacketQueue>(64);

puller->start(url, 
    [queue](const uint8_t* data, int size, int64_t pts) {
        RawPacketData packet(0, pts, data, size);
        if (!queue->push(std::move(packet))) {
            LOG_WARN("Queue full, dropping frame");
        }
    }
);
```

#### 示例 3：保存到文件

```cpp
std::ofstream out("stream.h264", std::ios::binary);

puller->start(url, [&out](const uint8_t* data, int size, int64_t pts) {
    // 写入 Annex B 格式（添加起始码）
    uint8_t start_code[] = {0x00, 0x00, 0x00, 0x01};
    out.write(reinterpret_cast<const char*>(start_code), 4);
    out.write(reinterpret_cast<const char*>(data), size);
});
```

---

## 📊 统计信息

### 获取统计

```cpp
// 运行中查询
uint64_t bytes = puller->bytes_received_.load();
uint64_t tags = puller->tags_processed_.load();
uint64_t frames = puller->frames_delivered_.load();

std::cout << "Bytes: " << bytes 
          << ", Tags: " << tags 
          << ", Frames: " << frames << std::endl;
```

### 重置统计

```cpp
// 在 stop() 时自动重置
puller->stop();

// 或手动重置
puller->reset();
```

---

## ⚠️ 错误处理

### 常见错误

#### 1. URL 解析失败

```
[Error] Invalid HTTP-FLV URL format: rtsp://127.0.0.1/live
```

**解决：** 使用正确的 HTTP-FLV 格式

#### 2. 连接失败

```
[Error] Connect failed: Connection refused (attempt 1)
```

**解决：**
- 检查 ZLMediaKit 是否运行
- 检查端口是否正确
- 检查防火墙设置

#### 3. FLV 头验证失败

```
[Error] Invalid FLV signature
```

**解决：**
- 确认流确实是 FLV 格式
- 检查是否有其他协议混用

---

## 🎯 完整示例

### 最小可用示例

```cpp
#include <iostream>
#include <thread>
#include "video_pipeline/puller/zlm_puller.h"

int main() {
    boost::asio::io_context io_ctx;
    
    auto puller = std::make_unique<ZLMPuller>(io_ctx);
    
    puller->start("http://127.0.0.1:8080/live/test.flv",
        [](const uint8_t* data, int size, int64_t pts) {
            std::cout << "Received: " << size << " bytes\n";
        });
    
    // 运行 10 秒
    std::thread io_thread([&io_ctx]() {
        io_ctx.run();
    });
    
    std::this_thread::sleep_for(std::chrono::seconds(10));
    
    puller->stop();
    io_thread.join();
    
    return 0;
}
```

### 带错误处理的完整示例

```cpp
#include <iostream>
#include <thread>
#include <csignal>
#include "video_pipeline/puller/zlm_puller.h"
#include "video_pipeline/frame_queue.h"
#include "log/logmanager.h"

std::atomic<bool> g_running{true};

void signalHandler(int) {
    g_running = false;
}

int main() {
    try {
        // 设置信号处理
        std::signal(SIGINT, signalHandler);
        
        // 初始化日志
        LogManager& log_mgr = LogManager::getInstance();
        log_mgr.Init();
        
        // 创建组件
        boost::asio::io_context io_ctx;
        auto puller = std::make_unique<ZLMPuller>(io_ctx);
        auto queue = std::make_shared<RawPacketQueue>(64);
        
        // 配置
        puller->setReconnectParams(3, -1);
        
        // 启动拉流
        std::string url = "http://127.0.0.1:8080/live/test.flv";
        bool success = puller->start(url,
            [&queue](const uint8_t* data, int size, int64_t pts) {
                RawPacketData packet(0, pts, data, size);
                if (!queue->push(std::move(packet))) {
                    static int dropped = 0;
                    if (++dropped % 100 == 0) {
                        LOG_WARN("Dropped {} frames", dropped);
                    }
                }
            }
        );
        
        if (!success) {
            LOG_MAIN_ERROR_AT("Failed to start puller");
            return 1;
        }
        
        LOG_MAIN_INFO_AT("Puller started");
        
        // 运行主循环
        std::thread io_thread([&io_ctx]() {
            io_ctx.run();
        });
        
        while (g_running && puller->isRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            // 打印统计
            LOG_MAIN_INFO_AT("Queue size: {}, Total frames: {}",
                           queue->size(),
                           queue->totalPopped());
        }
        
        // 清理
        puller->stop();
        io_thread.join();
        
        LOG_MAIN_INFO_AT("Shutdown complete");
        
    }
    catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("Exception: {}", e.what());
        return 1;
    }
    
    return 0;
}
```

---

## 🔍 调试技巧

### 1. 启用详细日志

```cpp
// 设置日志级别为 debug
config.log_level = 0;

// 或在配置文件中
logs:
  mainlog:
    level: debug
```

### 2. 查看关键日志

```bash
# 过滤相关日志
tail -f app.log | grep -E "ZLMPuller|FLV|NALU"
```

### 3. 使用 Wireshark 抓包

```bash
# 捕获 HTTP-FLV 流量
wireshark -i eth0 -f "tcp port 8080"
```

### 4. 测试流地址

```bash
# 使用 ffplay 测试
ffplay "http://127.0.0.1:8080/live/test.flv"

# 使用 VLC 测试
vlc "http://127.0.0.1:8080/live/test.flv"
```

---

## 📈 性能调优

### 队列大小调整

```cpp
// 低延迟场景
auto queue = std::make_shared<RawPacketQueue>(16);

// 高吞吐场景
auto queue = std::make_shared<RawPacketQueue>(128);

// 网络不稳定
auto queue = std::make_shared<RawPacketQueue>(256);
```

### 回调优化

```cpp
// ❌ 避免在回调中做耗时操作
puller->start(url, [](const uint8_t* data, int size, int64_t pts) {
    cv::Mat result = heavyProcessing(data, size);  // 慢！
    display(result);                                // 更慢！
});

// ✅ 只负责传递数据到队列
puller->start(url, [queue](const uint8_t* data, int size, int64_t pts) {
    RawPacketData packet(0, pts, data, size);
    queue->push(std::move(packet));  // 快！
});
```

---

## 🆘 故障排查清单

- [ ] ZLMediaKit 是否正常运行？
- [ ] 流地址是否正确？
- [ ] 端口是否可访问？
- [ ] 防火墙是否允许？
- [ ] FLV 格式是否正确？
- [ ] 回调函数是否被调用？
- [ ] 统计信息是否在增长？
- [ ] 日志中是否有错误信息？

---

## 📖 相关文档

- [实现总结](./ZLM_PULLER_IMPLEMENTATION.md) - 详细的架构设计
- [FrameQueue](../frame_queue.h) - 无锁队列使用
- [IPuller](./i_puller.h) - 拉流器接口定义
