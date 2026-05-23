# Stream Puller 模块设计文档

## 架构总览

```
┌──────────────────────────────────────────────────────────────┐
│                      StreamSource                            │
│   - 持有 shared_ptr<StreamSession>                          │
│   - 缓存 StreamInfo                                          │
│   - 将 MediaPacket 广播给 N 个订阅者                        │
│   - 将配置委派给 session/puller 层                           │
└──────────────┬───────────────────────────────────────────────┘
               │ 持有
┌──────────────▼───────────────────────────────────────────────┐
│                      StreamSession                           │
│   - 持有 unique_ptr<IPuller>                                │
│   - 状态机 (KIDLE → KCONNECTING → KCONNECTED …)             │
│   - ReadLoop 通过 boost::asio::post 调度（无专属线程）       │
│   - DoReconnect 通过 steady_timer 异步触发                   │
│   - Watchdog 通过 steady_timer 定期检测                      │
│   - 统计（字节数/包数/码率）                                  │
└──────────────┬───────────────────────────────────────────────┘
               │ 调用
┌──────────────▼───────────────────────────────────────────────┐
│                      IPuller（纯虚接口）                       │
│   Open(url) / Close() / ReadPacket() / GetStreamInfo()      │
└──────────────┬───────────────────────────────────────────────┘
               │ 实现者
┌──────────────▼───────────────────────────────────────────────┐
│                      FFmpegPuller                            │
│   - avformat_open_input / av_read_frame / avformat_close_input│
│   - AVPacket 对象池复用                                      │
│   - 视频包：池 → av_packet_alloc + move_ref                  │
│   - 中断回调实现超时控制                                      │
└──────────────────────────────────────────────────────────────┘
```

## 各层职责

### 1. `IPuller` — 协议适配器接口

**文件：** `modules/media/puller/include/puller/i_puller.hpp`

纯虚接口，定义任意拉流协议实现的最小协定。

| 方法 | 职责 |
|--------|---------|
| `Open(url)` | 建立底层传输连接 |
| `Close()` | 断开底层连接 |
| `ReadPacket(packet)` | 读取一个媒体包（阻塞） |
| `GetStreamInfo()` | 返回缓存的流元数据 |
| `SetEventCallback(cb)` | 注册协议层事件通知 |

**明确不负责：** 重连、watchdog、状态机、统计、解码器、pipeline —— 这些全部属于 `StreamSession` 或 `StreamSource`。

### 2. `FFmpegPuller` — FFmpeg 实现

**文件：**
- `modules/media/puller/include/puller/ffmpeg_puller.hpp`
- `modules/media/puller/src/ffmpeg_puller.cpp`

使用 FFmpeg `libavformat` 的 `IPuller` 具体实现，支持 RTSP / HTTP / FLV 等协议。

#### 生命周期

```
Open(url)
  → avformat_alloc_context()
  → 设置 interrupt_callback（超时）
  → avformat_open_input()
  → avformat_find_stream_info()
  → 选择第一个视频流
  → 缓存 StreamInfo
  → 返回 true/false

Close()
  → 设置中断标志
  → avformat_close_input()
  → 清空缓存信息

ReadPacket(packet)
  → av_read_frame()
  → 非目标流：unref + 归还池，返回空包
  → 视频流：av_packet_move_ref(池 → av_packet_alloc 分配)
  → EOF/错误：unref + 归还池，返回 false
```

#### 池 → `av_packet_alloc` 交接策略

`FFmpegPacketBuffer` 析构函数调用 `av_packet_free(&pkt_)` → `av_freep()` → `av_free()` → `free()`。这要求 `AVPacket` 必须是 `av_packet_alloc()` 分配（使用 `av_malloc()`），不能是 `boost::pool::malloc()` 分配的内存。

**`ReadPacket()` 中的处理逻辑：**
1. 从池中分配临时 `AVPacket`
2. `av_read_frame()` 填充池包
3. 非目标流：`av_packet_unref()` + 归还池
4. 视频流：`av_packet_alloc()` 分配新包，`av_packet_move_ref(owned, pool_pkt)` 转移 buffer/side-data 所有权，然后 `av_packet_unref()` + 归还临时包
5. 将 owned 包包装为 `FFmpegPacketBuffer` → 析构时 `av_packet_free` 安全

#### 配置方法

| 方法 | 效果 |
|--------|--------|
| `SetConnectTimeoutMs(ms)` | 连接阶段中断回调阈值 |
| `SetReadTimeoutMs(ms)` | FFmpeg 的 `stimeout` 选项 |
| `SetLowLatency(bool)` | `fflags=nobuffer` + `flags=low_delay` |
| `SetCredentials(u, p)` | `user` / `password` 字典项 |

### 3. `StreamSession` — 连接生命周期管理器

**文件：**
- `modules/media/stream/include/stream/session/stream_session.h`
- `modules/media/stream/src/stream_session.cpp`

管理一次拉流会话的完整生命周期：连接、读取、自动重连、停止、watchdog。

#### 状态机

```
         KIDLE
           │ Start()
           ▼
     KCONNECTING
           │ Open() 成功
           ▼
     KCONNECTED ◄──────────────────────────┐
           │    ▲                          │
      ReadLoop  │                    DoReconnect()
      失败      │ Connect() 成功             │
           │    │                          │
           ▼    │                          │
     KRECONNECTING──────────────────────────┘
           │ 达到最大重连次数
           ▼
         KERROR

  任意状态 → Stop() → KSTOPPED
```

#### 线程模型

`StreamSession` **不持有**线程。`ReadLoop` 通过 `boost::asio::post(io_, ...)` 分发到构造时传入的 `io_context` 上，由外部线程池（或 `io_context::run()`）驱动。这样设计的好处：

- 多个 session 共享同一批 I/O 线程
- `ReadPacket()` 可能阻塞（同步 FFmpeg 读取）—— 外部多线程 io_context 确保定时器（watchdog、reconnect）保持响应
- session 内部不创建/销毁 `std::thread`

#### ReadLoop 流程

```
ReadLoop():
  如果 !running → 返回
  packet = puller_->ReadPacket()
  如果 !running → 返回
  如果 ok:
    如果 packet 为空（非目标流）→ 立即 post(ReadLoop)
    否则：
      更新统计，记录 last_read_time
      调用 packet_cb_
      post(ReadLoop)
  否则：
    DoReconnect()
```

关键设计点：每次 `ReadLoop` 只读取一个包，然后重新 post 自身。这样 I/O 执行器有机会在两个包读取之间运行其他处理器（定时器、其他 session）。

#### 重连

```
DoReconnect():
  puller_->Close()
  检查 max_reconnect_count
  递增 reconnect_count
  reconnect_timer_.expires_after(间隔)
  async_wait → Connect()

Connect():
  puller_->Open(url_)
  如果失败 → DoReconnect()（递归）
  重置 reconnect_count
  分发 StreamInfo
  设置状态为 KCONNECTED
  post(ReadLoop)
  启动 Watchdog
```

#### Watchdog

采用 `steady_timer` 链式调用的机制，定时检查 `last_read_time_`。如果空闲时间超过 `watchdog_interval_ms_`，触发 `DoReconnect()`。当 `watchdog_interval_ms_ == 0` 时关闭。

#### 统计

原子变量累加周期内的字节数/包数；`GetStats()` 返回快照，包含：
- `bytes_received`
- `packets_received`
- `bitrate`（占位，当前为 0）
- `reconnect_count`

### 4. `StreamSource` — 媒体源 / 订阅者管理器

**文件：**
- `modules/media/stream/include/stream/stream_source.h`
- `modules/media/stream/src/stream_source.cpp`

对外代表一路媒体流。持有 `shared_ptr<StreamSession>`，负责包分发。

#### 职责分配

| 关注点 | 委派给 |
|---------|----------------|
| 连接生命周期 | `StreamSession` |
| 包分发 | `StreamSource` 广播给 `AddPacketSubscriber()` 注册的回调 |
| 元数据缓存 | `StreamSource::GetStreamInfo()` 返回缓存的 `StreamInfo` |
| 配置 | `SetStreamSourceConfig()` 存储配置；`ApplyConfig()` 应用到 session |

#### 订阅者模型

- `AddPacketSubscriber(cb)` 追加到 `vector<PacketCallback>`
- `OnPacket()` 在 `subscriber_mutex_` 保护下遍历，逐个调用回调
- 所有订阅者收到同一个 `shared_ptr<MediaPacket>` 对象（共享语义）
- 不支持删除（仅增长；临时订阅者建议用 weak_ptr 包装）

#### ApplyConfig

`StreamSourceConfig` 拆分为 `SessionConfig` 和 `PullerConfig`：

```cpp
struct StreamSourceConfig {
    struct SessionConfig {
        int connect_timeout_ms;
        int read_timeout_ms;
        int reconnect_interval_ms;
        int max_reconnect_count;
        int watchdog_interval_ms;
    } session;

    struct PullerConfig {
        int io_timeout_ms;
        bool low_latency;
        // ...
    } puller;
};
```

- Session 配置直接通过 `session_->SetReconnectIntervalMs(...)` 等接口应用
- Puller 配置（FFmpeg 特有）当前为占位——设计期望 puller 在注入 session 之前预先配置好。`ApplyConfig()` 中预留了 `dynamic_cast<FFmpegPuller*>` 方案供后续实现

## 数据流示例

```
外部代码                     StreamSource              StreamSession        FFmpegPuller
     │                              │                        │                    │
     │ SetSession(session)          │                        │                    │
     │ SetStreamSourceConfig(cfg)   │                        │                    │
     │ Start() ──────────────────►  │                        │                    │
     │                              │ ApplyConfig()          │                    │
     │                              │ SetPacketCallback()    │                    │
     │                              │ SetStreamInfoCallback()│                    │
     │                              │ session_->Start() ───► │                    │
     │                              │                        │ puller_->Open() ──►│
     │                              │                        │◄── true/false ────│
     │                              │◄── info_cb(info) ─────│                    │
     │                              │ post(ReadLoop)         │                    │
     │                              │                        │ puller_->Read() ──►│
     │                              │◄── pkt_cb(packet) ────│◄── MediaPacket ◄───│
     │ AddPacketSubscriber(cb)      │                        │                    │
     │ cb(packet) ◄─────────────────│                        │                    │
     │                              │                        │ puller_->Read() ──►│
     │                              │◄── pkt_cb(packet) ────│◄── MediaPacket ◄───│
     │ cb(packet) ◄─────────────────│                        │                    │
     │                              │                        │ ...                │
     │ Stop() ────────────────────► │                        │                    │
     │                              │ session_->Stop() ────►│                    │
     │                              │                        │ puller_->Close() ─►│
     │                              │                        │ state = KSTOPPED   │
```

## 构建集成

源码分布三个目录，统一编译到 `media_lib` CMake target：

```
modules/media/
├── defines/
│   ├── include/defines/   → media_packet.hpp, ffmpeg_packet_buffer.hpp, ...
│   └── src/               → ffmpeg_packet_buffer.cpp, ...
├── puller/
│   ├── include/puller/    → i_puller.hpp, ffmpeg_puller.hpp
│   └── src/               → ffmpeg_puller.cpp
└── stream/
    ├── include/stream/        → stream_source.h, stream_info.h
    │              /session/   → stream_session.h, source_config.h
    └── src/                   → stream_source.cpp, stream_session.cpp
```

`defines/CMakeLists.txt` 将 `defines/src/*.cpp` 和 `stream/src/*.cpp` 全部 glob 到 `media_lib` 中，并将 `defines/include`、`stream/include`、`puller/include` 设为 public 包含路径。

## 线程安全

| 组件 | 线程安全性 |
|-----------|--------------|
| `IPuller::ReadPacket()` | 由 ReadLoop 内调用（通过 io_context 形成逻辑单线程）。阻塞调用——不可在单 I/O 线程环境中独占。 |
| `FFmpegPuller` | 非线程安全。所有方法应顺序调用。`Close()` 设置原子中断标志，可能被另一个线程上的中断回调观察到。 |
| `StreamSession` | `Set*` 方法（配置、回调）：通过 `cb_mutex_` 保护回调，使用原子变量管理状态。`ReadLoop`/`DoReconnect`/`StartWatchdog`：全部分发到同一个 `io_context`，实际为单线程执行。 |
| `StreamSource` | `AddPacketSubscriber()`/`OnPacket()` 均加 `subscriber_mutex_`。`Start()`/`Stop()` 不可重入——期望由单线程调用。 |

## 错误处理策略

| 场景 | 处理方式 |
|----------|----------|
| `IPuller::ReadPacket()` 返回 false | Session 视为 EOF/错误 → `DoReconnect()` |
| Puller `Open()` 失败 | Session 进入 `KERROR` |
| 读超时（间隔内无数据） | Watchdog 检测到 → `DoReconnect()` |
| 超过最大重连次数 | Session 进入 `KERROR`（永久错误） |
| `Stop()` 在重连过程中触发 | `Connect()` 入口检查 `running_` 标志 → 实际不执行任何操作 |

## 预留扩展点（设计预留）

- **解码器管理**：`StreamSource::OnSessionState()` 预留了在状态转换时挂载/卸载解码器的入口
- **Pipeline 管理**：`StreamSource` 未来可持有 filter 链（解码 → 缩放 → 编码）
- **Sink 管理**：输出到多个渲染器/录像器
- **Puller 配置注入**：`ApplyConfig()` 中预留了 `dynamic_cast<FFmpegPuller*>` 的代码框架，用于将 `StreamSourceConfig::PullerConfig` 传递到 puller
