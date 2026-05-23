# Stream Puller Modules Design

## Architecture Overview

```
┌──────────────────────────────────────────────────────────────┐
│                      StreamSource                            │
│   - owns shared_ptr<StreamSession>                           │
│   - caches StreamInfo                                        │
│   - broadcasts MediaPacket to N subscribers                  │
│   - delegates config to session/puller layers                │
└──────────────┬───────────────────────────────────────────────┘
               │ holds
┌──────────────▼───────────────────────────────────────────────┐
│                      StreamSession                           │
│   - owns unique_ptr<IPuller>                                 │
│   - state machine (KIDLE → KCONNECTING → KCONNECTED …)      │
│   - ReadLoop via boost::asio::post (no dedicated thread)     │
│   - DoReconnect via steady_timer                             │
│   - Watchdog via steady_timer                                │
│   - stats (bytes/packets/bitrate)                            │
└──────────────┬───────────────────────────────────────────────┘
               │ calls
┌──────────────▼───────────────────────────────────────────────┐
│                      IPuller (abstract)                       │
│   Open(url) / Close() / ReadPacket() / GetStreamInfo()      │
└──────────────┬───────────────────────────────────────────────┘
               │ implemented by
┌──────────────▼───────────────────────────────────────────────┐
│                      FFmpegPuller                            │
│   - avformat_open_input / av_read_frame / avformat_close_input│
│   - AVPacket ObjectPool for allocation reuse                 │
│   - video pack: pool → av_packet_alloc + move_ref            │
│   - interrupt callback for timeout control                   │
└──────────────────────────────────────────────────────────────┘
```

## Layer Responsibilities

### 1. `IPuller` — Protocol Adapter Interface

**File:** `modules/media/puller/include/puller/i_puller.hpp`

Pure abstract interface that defines the minimum contract for any stream protocol implementation.

| Method | Purpose |
|--------|---------|
| `Open(url)` | Establish underlying transport connection |
| `Close()` | Tear down transport connection |
| `ReadPacket(packet)` | Read one media packet (blocking) |
| `GetStreamInfo()` | Return cached stream metadata |
| `SetEventCallback(cb)` | Register protocol-level event notification |

**Deliberately excluded:** reconnect, watchdog, state machine, statistics, decoder, pipeline — all belong to `StreamSession` or `StreamSource`.

### 2. `FFmpegPuller` — FFmpeg Implementation

**Files:**
- `modules/media/puller/include/puller/ffmpeg_puller.hpp`
- `modules/media/puller/src/ffmpeg_puller.cpp`

Concrete `IPuller` implementation using FFmpeg's `libavformat` for RTSP/HTTP/FLV etc.

#### Lifecycle

```
Open(url)
  → avformat_alloc_context()
  → set interrupt_callback (timeout)
  → avformat_open_input()
  → avformat_find_stream_info()
  → select first video stream
  → cache StreamInfo
  → return true/false

Close()
  → set interrupt flag
  → avformat_close_input()
  → clear cached info

ReadPacket(packet)
  → av_read_frame()
  → if non-target stream: unref + deallocate to pool, return (empty packet)
  → if video stream: av_packet_move_ref(pool → av_packet_alloc'd), return (owned packet)
  → on EOF/error: unref + deallocate to pool, return false
```

#### Pool → `av_packet_alloc` Handoff

`FFmpegPacketBuffer` destructor calls `av_packet_free(&pkt_)` → `av_freep()` → `av_free()` → `free()`. This requires `AVPacket` to be allocated by `av_packet_alloc()` (which uses `av_malloc()`), NOT by `boost::pool::malloc()`.

**Strategy in `ReadPacket()`:**
1. Pool-allocate a temporary `AVPacket`
2. `av_read_frame()` fills the pool packet
3. Non-video packets: `av_packet_unref()` + pool deallocate (skip)
4. Video packets: `av_packet_alloc()` a new owned packet, `av_packet_move_ref(owned, pool_pkt)` transfers buffer/side-data ownership, then `av_packet_unref()` + pool deallocate the temp
5. The owned packet is wrapped in `FFmpegPacketBuffer` → safe `av_packet_free` on destruction

#### Configuration Setters

| Setter | Effect |
|--------|--------|
| `SetConnectTimeoutMs(ms)` | Interrupt callback threshold during connection |
| `SetReadTimeoutMs(ms)` | `stimeout` option for FFmpeg |
| `SetLowLatency(bool)` | `fflags=nobuffer` + `flags=low_delay` |
| `SetCredentials(u, p)` | `user`/`password` dict entries |

### 3. `StreamSession` — Connection Lifecycle Manager

**Files:**
- `modules/media/stream/include/stream/session/stream_session.h`
- `modules/media/stream/src/stream_session.cpp`

Manages a single stream connection: connect, read, auto-reconnect, stop, watchdog.

#### State Machine

```
         KIDLE
           │ Start()
           ▼
     KCONNECTING
           │ Open() succeeds
           ▼
     KCONNECTED ◄──────────────────────────┐
           │    ▲                          │
      ReadLoop  │                    DoReconnect()
      fails     │ Connect() succeeds       │
           │    │                          │
           ▼    │                          │
     KRECONNECTING──────────────────────────┘
           │ max reconnect reached
           ▼
         KERROR

  Any state → Stop() → KSTOPPED
```

#### Thread Model

`StreamSession` does **not** own a thread. The `ReadLoop` is dispatched via `boost::asio::post(io_, ...)` onto the `io_context` passed at construction. An external thread pool (or `io_context::run()`) drives execution. This allows:

- Multiple sessions sharing the same pool of I/O threads
- `ReadPacket()` may block (synchronous FFmpeg read) — external multithreaded io_context ensures timers (watchdog, reconnect) remain responsive
- No `std::thread` creation/destruction inside the session

#### ReadLoop Flow

```
ReadLoop():
  if !running → return
  packet = puller_->ReadPacket()
  if !running → return
  if ok:
    if packet is null (non-target stream) → post(ReadLoop) immediately
    else:
      update stats, record last_read_time
      invoke packet_cb_
      post(ReadLoop)
  else:
    DoReconnect()
```

Key design point: each invocation of `ReadLoop` reads exactly one packet, then re-posts itself. This gives the I/O executor a chance to run other handlers (timers, other sessions) between packet reads.

#### Reconnect

```
DoReconnect():
  puller_->Close()
  check max_reconnect_count
  increment reconnect_count
  reconnect_timer_.expires_after(interval)
  async_wait → Connect()

Connect():
  puller_->Open(url_)
  if fails → DoReconnect() (recursive)
  reset reconnect_count
  distribute StreamInfo
  set state KCONNECTED
  post(ReadLoop)
  start Watchdog
```

#### Watchdog

A `steady_timer` chain that checks `last_read_time_`. If idle exceeds `watchdog_interval_ms_`, triggers `DoReconnect()`. Disabled when `watchdog_interval_ms_ == 0`.

#### Stats

Atomics accumulate bytes/packets per period; `GetStats()` returns a snapshot with:
- `bytes_received`
- `packets_received`
- `bitrate` (placeholder, currently 0)
- `reconnect_count`

### 4. `StreamSource` — Media Source / Subscriber Manager

**Files:**
- `modules/media/stream/include/stream/stream_source.h`
- `modules/media/stream/src/stream_source.cpp`

Represents one media stream to the outside world. Holds a `shared_ptr<StreamSession>` and manages packet distribution.

#### Responsibilities

| Concern | Where delegated |
|---------|----------------|
| Connection lifecycle | `StreamSession` |
| Packet distribution | `StreamSource` broadcasts to `AddPacketSubscriber()` callbacks |
| Metadata cache | `StreamSource::GetStreamInfo()` returns cached `StreamInfo` |
| Configuration | `SetStreamSourceConfig()` stores config; `ApplyConfig()` applies to session |

#### Subscriber Model

- `AddPacketSubscriber(cb)` appends to `vector<PacketCallback>`
- `OnPacket()` iterates under `subscriber_mutex_`, invokes each callback
- All subscribers receive `shared_ptr<MediaPacket>` (same object)
- Removal is not supported (grow-only; for ephemeral subscribers, wrap with a weak_ptr guard)

#### ApplyConfig

`StreamSourceConfig` splits into `SessionConfig` and `PullerConfig`:

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

- Session config is applied directly via `session_->SetReconnectIntervalMs(...)` etc.
- Puller config (FFmpeg-specific) is a placeholder — the current design expects the puller to be pre-configured before being injected into the session. A `dynamic_cast<FFmpegPuller*>` approach in `ApplyConfig()` is reserved for future use.

## Data Flow Example

```
External Code                  StreamSource              StreamSession        FFmpegPuller
     │                              │                        │                    │
     │ SetSession(session)          │                        │                    │
     │ SetStreamSourceConfig(cfg)   │                        │                    │
     │ Start() ──────────────────►  │                        │                    │
     │                              │ ApplyConfig()          │                    │
     │                              │ SetPacketCallback()    │                    │
     │                              │ SetStreamInfoCallback()│                    │
     │                              │ session_->Start() ───► │                    │
     │                              │                        │ puller_->Open() ──►│
     │                              │                        │◄─── true/false ────│
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

## Build Integration

Sources live in three directories, all merged into the `media_lib` CMake target:

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

`defines/CMakeLists.txt` globs all `defines/src/*.cpp` and `stream/src/*.cpp` into `media_lib`, and exposes `defines/include`, `stream/include`, and `puller/include` as public include paths.

## Thread Safety

| Component | Thread Safety |
|-----------|--------------|
| `IPuller::ReadPacket()` | Called from ReadLoop (single logical thread via io_context). Blocking — must not block the only I/O thread. |
| `FFmpegPuller` | Not thread-safe. All methods should be called sequentially. `Close()` sets atomic interrupt flag which can be observed by the interrupt callback on another thread. |
| `StreamSession` | `Set*` methods (config, callbacks): externally synchronized via `cb_mutex_` for callbacks, atomics for state. `ReadLoop`/`DoReconnect`/`StartWatchdog`: all dispatched on the same `io_context`, effectively single-threaded. |
| `StreamSource` | `AddPacketSubscriber()`/`OnPacket()` both lock `subscriber_mutex_`. `Start()`/`Stop()` not reentrant — expected to be called from single thread. |

## Error Handling Strategy

| Scenario | Handling |
|----------|----------|
| `IPuller::ReadPacket()` returns false | Session treats as EOF/error → `DoReconnect()` |
| Puller `Open()` fails | Session enters `KERROR` |
| Read timeout (no data within interval) | Watchdog detects → `DoReconnect()` |
| Max reconnects exceeded | Session enters `KERROR` (permanent) |
| `Stop()` during reconnecting | `running_` flag checked before `Connect()` → effectively no-op |

## Future Extensions (Design-Reserved)

- **Decoder management**: `StreamSource::OnSessionState()` is a placeholder for attaching/detaching decoders on state transitions
- **Pipeline management**: `StreamSource` can own a pipeline of filters (decode → scale → encode)
- **Sink management**: Output to multiple renderers/recorders
- **Puller config injection**: `ApplyConfig()` has a placeholder for `dynamic_cast<FFmpegPuller*>` to pass `StreamSourceConfig::PullerConfig` through to the puller
