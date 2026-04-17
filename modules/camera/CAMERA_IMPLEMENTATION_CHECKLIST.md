# Camera 模块实现清单

## 📋 Phase 1: 基础框架（优先级最高）

### 1.1 CameraStorage 实现

**文件：** `src/camera/camera_storage.cpp`

- [ ] 实现单例模式 `GetInstance()`
- [ ] 实现 `Init(const std::string& db_path)` - 初始化 SQLite 数据库
- [ ] 实现 `CreateTable()` - 创建 cameras 表
- [ ] 实现 `Add(const CameraInfo&)` - 插入摄像头
- [ ] 实现 `Remove(const std::string& uuid)` - 删除摄像头
- [ ] 实现 `Update(const CameraInfo&)` - 更新摄像头
- [ ] 实现 `Get(const std::string& uuid, CameraInfo&)` - 查询单个
- [ ] 实现 `GetAll(std::vector<CameraInfo>&)` - 查询全部
- [ ] 实现 `UpdateStatus(const std::string& uuid, CameraStatus)` - 更新状态
- [ ] 实现密码加密/解密函数

**测试：**
- [ ] 单元测试：增删改查
- [ ] 测试密码加密/解密
- [ ] 测试并发访问

---

### 1.2 CameraInfo 完善

**文件：** `include/camera/camera.h`

- [ ] 添加 `protocol_type` 字段
- [ ] 添加 `http_base_url` 字段
- [ ] 添加 `onvif_device_url` 字段
- [ ] 添加 `gb28181_id` 字段
- [ ] 添加 `last_online_time` 字段
- [ ] 添加 `offline_count` 字段
- [ ] 实现 `ToJsonObject()` 方法
- [ ] 实现 `FromJsonObject()` 方法（反向解析）

---

### 1.3 配置文件支持

**文件：** `include/config/common_config.h`

- [ ] 确认 `CameraDbConfig` 结构完整
- [ ] 添加 `camera.encryption_key` 配置项
- [ ] 添加 `camera.auto_start_stream` 配置项
- [ ] 添加 `camera.health_check_interval` 配置项

**示例配置：**
```yaml
camera_db:
  db_path: "./data/camera.db"
  
camera:
  encryption_key: "your-secret-key-here"  # AES-256 密钥
  auto_start_stream: true
  health_check_interval: 30  # 秒
```

---

## 📋 Phase 2: CameraManager 和 CameraDevice

### 2.1 CameraManager 实现

**文件：** `src/camera/camera_manager.cpp`

- [ ] 实现单例模式 `GetInstance()`
- [ ] 实现 `Init()` - 从数据库加载所有摄像头
- [ ] 实现 `Shutdown()` - 停止所有摄像头
- [ ] 实现 `Register(const CameraInfo&)` - 注册新摄像头
- [ ] 实现 `Unregister(const std::string& uuid)` - 注销摄像头
- [ ] 实现 `GetCamera(const std::string& uuid)` - 获取摄像头对象
- [ ] 实现 `GetAllCameras()` - 获取所有摄像头
- [ ] 实现 `StartAllCameras()` - 批量启动
- [ ] 实现 `StopAllCameras()` - 批量停止
- [ ] 实现 `SetStatusCallback()` - 设置状态回调
- [ ] 实现健康检查线程 `HealthCheckLoop()`

**测试：**
- [ ] 测试从数据库加载摄像头
- [ ] 测试注册/注销
- [ ] 测试批量启动/停止
- [ ] 测试健康检查

---

### 2.2 CameraDevice 实现

**文件：** `src/camera/camera_manager.cpp`（与 CameraManager 同文件）

- [ ] 实现构造函数 `CameraDevice(const CameraInfo&)`
- [ ] 实现析构函数 `~CameraDevice()`
- [ ] 实现 `Start()` - 启动摄像头
- [ ] 实现 `Stop()` - 停止摄像头
- [ ] 实现 `Restart()` - 重启摄像头
- [ ] 实现 `GetInfo()` - 获取摄像头信息
- [ ] 实现 `GetStatus()` - 获取状态
- [ ] 实现 `UpdateStatus(CameraStatus)` - 更新状态
- [ ] 实现 `InitializeProtocol()` - 初始化协议适配器
- [ ] 实现 `SendCommand()` - 发送 HTTP 命令

**临时实现（Phase 1-2）：**
```cpp
bool CameraDevice::Start() {
    // 暂时只更新状态，不实际连接
    UpdateStatus(CameraStatus::Online);
    return true;
}

bool CameraDevice::Stop() {
    UpdateStatus(CameraStatus::Offline);
    return true;
}
```

---

## 📋 Phase 3: CameraHttpClient

### 3.1 基础 HTTP 客户端

**文件：** `src/camera/camera_httpclient.cpp`

- [ ] 实现构造函数（初始化 HTTP 连接池）
- [ ] 实现 `Get(const std::string& api, boost::json::object&)`
- [ ] 实现 `Post(const std::string& api, const boost::json::object&, boost::json::object&)`
- [ ] 实现 `BuildUrl(const std::string& api)` - 构建完整 URL
- [ ] 实现 `BuildAuthHeader()` - 构建认证头（Basic Auth）
- [ ] 实现 `ParseResponse()` - 解析响应

**依赖：** 需要 `net_lib` 中的 `PooledClient`

---

### 3.2 设备控制 API

- [ ] 实现 `GetDeviceInfo(boost::json::object&)`
- [ ] 实现 `SetVideoParams(int width, int height, int fps)`
- [ ] 实现 `RebootDevice()`
- [ ] 实现 `PTZMove(int direction, int speed)`
- [ ] 实现 `GetSnapshot(const std::string& save_path)`

**厂商适配：**
- [ ] 海康威视 API 路径映射
- [ ] 大华 API 路径映射

**测试：**
- [ ] 使用真实摄像头测试（或模拟器）
- [ ] 测试认证失败场景
- [ ] 测试网络超时场景

---

## 📋 Phase 4: StreamManager

### 4.1 StreamSession 实现

**文件：** `src/camera/camera_stream.cpp`

- [ ] 实现构造函数 `StreamSession(const CameraInfo&)`
- [ ] 实现析构函数 `~StreamSession()`
- [ ] 实现 `Start()` - 开始拉流（FFmpeg）
- [ ] 实现 `Stop()` - 停止拉流
- [ ] 实现 `IsRunning()` - 查询运行状态
- [ ] 实现 `GetStats()` - 获取统计信息
- [ ] 实现 `OnDataReceived()` - 数据接收回调
- [ ] 实现 `OnError()` - 错误处理回调
- [ ] 实现自动重连逻辑

**依赖：** 需要 `ffmpeg_opt_lib` 中的 `FFmpegDemuxer`

---

### 4.2 StreamManager 实现

- [ ] 实现单例模式 `GetInstance()`
- [ ] 实现 `Init()` - 初始化
- [ ] 实现 `Shutdown()` - 关闭所有流
- [ ] 实现 `StartStream(const CameraInfo&)` - 开始拉流
- [ ] 实现 `StopStream(const std::string& uuid)` - 停止拉流
- [ ] 实现 `IsStreaming(const std::string& uuid)` - 查询状态
- [ ] 实现 `GetStats(const std::string& uuid)` - 获取统计

**集成 ZLMediaKit：**
- [ ] 拉流后推送到 ZLMediaKit
- [ ] 生成播放地址（RTSP/RTMP/HTTP-FLV/HLS/WebRTC）

**测试：**
- [ ] 测试 RTSP 拉流
- [ ] 测试断线重连
- [ ] 测试多路并发拉流

---

## 📋 Phase 5: 协议适配器（可选，后期实现）

### 5.1 ProtocolAdapter 基类

**文件：** `include/camera/protocol_adapter.h`

- [ ] 定义纯虚接口
- [ ] 定义通用数据结构

---

### 5.2 OnvifAdapter（后期）

**依赖库：** gSOAP 或 libonvif

- [ ] 实现 WS-Discovery 发现
- [ ] 实现 ONVIF Device Service
- [ ] 实现 ONVIF Media Service
- [ ] 实现 ONVIF PTZ Service

---

### 5.3 GB28181Adapter（后期）

**依赖库：** PJSIP

- [ ] 实现 SIP REGISTER
- [ ] 实现 SIP INVITE
- [ ] 实现 RTP 接收
- [ ] 实现 PS 流解复用

---

## 📋 Phase 6: Web API 集成

### 6.1 RESTful API

**文件：** `modules/web/src/api/camera_api_handler.cpp`（新建）

- [ ] 实现 `GET /api/cameras` - 获取所有摄像头
- [ ] 实现 `GET /api/cameras/{uuid}` - 获取单个摄像头
- [ ] 实现 `POST /api/cameras` - 添加摄像头
- [ ] 实现 `PUT /api/cameras/{uuid}` - 更新摄像头
- [ ] 实现 `DELETE /api/cameras/{uuid}` - 删除摄像头
- [ ] 实现 `POST /api/cameras/discover` - ONVIF 发现
- [ ] 实现 `POST /api/cameras/{uuid}/start` - 启动摄像头
- [ ] 实现 `POST /api/cameras/{uuid}/stop` - 停止摄像头
- [ ] 实现 `POST /api/cameras/{uuid}/stream/start` - 开始拉流
- [ ] 实现 `POST /api/cameras/{uuid}/stream/stop` - 停止拉流
- [ ] 实现 `POST /api/cameras/{uuid}/ptz/move` - 云台控制
- [ ] 实现 `POST /api/cameras/{uuid}/snapshot` - 截图

---

### 6.2 WebSocket 事件推送

**文件：** `modules/web/src/ws/camera_event_handler.cpp`（新建）

- [ ] 实现状态变化事件推送
- [ ] 实现流状态事件推送
- [ ] 实现错误事件推送

---

## 🔧 开发环境准备

### 必需的软件

- [ ] CMake 3.18+
- [ ] Visual Studio 2022 / GCC 11+
- [ ] SQLite3
- [ ] FFmpeg 6.x
- [ ] Boost 1.90
- [ ] OpenSSL 3.x

### 可选的软件（用于测试）

- [ ] ONVIF Device Manager（测试 ONVIF）
- [ ] VLC（测试 RTSP 流）
- [ ] Postman（测试 API）
- [ ] Wireshark（抓包调试）

---

## 📊 进度跟踪

| Phase | 任务数 | 完成数 | 进度 |
|-------|--------|--------|------|
| Phase 1 | 20 | 0 | 0% |
| Phase 2 | 20 | 0 | 0% |
| Phase 3 | 15 | 0 | 0% |
| Phase 4 | 20 | 0 | 0% |
| Phase 5 | 15 | 0 | 0% |
| Phase 6 | 15 | 0 | 0% |
| **总计** | **105** | **0** | **0%** |

---

## 🎯 快速开始建议

### Week 1: 完成 Phase 1

**目标：** 可以手动添加/删除摄像头，数据持久化到 SQLite

**关键任务：**
1. 实现 `CameraStorage`
2. 完善 `CameraInfo`
3. 编写单元测试

**验收标准：**
- ✅ 可以添加摄像头到数据库
- ✅ 可以从数据库查询摄像头
- ✅ 密码加密存储

---

### Week 2: 完成 Phase 2

**目标：** 可以启动/停止摄像头，状态管理正常

**关键任务：**
1. 实现 `CameraManager`
2. 实现 `CameraDevice` 基础功能
3. 实现健康检查

**验收标准：**
- ✅ 可以从数据库加载所有摄像头
- ✅ 可以启动/停止摄像头
- ✅ 状态变化有日志记录

---

### Week 3: 完成 Phase 3

**目标：** 可以远程控制摄像头（HTTP API）

**关键任务：**
1. 实现 `CameraHttpClient`
2. 适配海康/大华 API
3. 测试云台控制

**验收标准：**
- ✅ 可以查询设备信息
- ✅ 可以设置视频参数
- ✅ 可以云台控制
- ✅ 可以截图

---

### Week 4: 完成 Phase 4

**目标：** 可以拉取 RTSP 流并推送到 ZLMediaKit

**关键任务：**
1. 实现 `StreamSession`
2. 实现 `StreamManager`
3. 集成 ZLMediaKit

**验收标准：**
- ✅ 可以拉取 RTSP 流
- ✅ 可以推送到 ZLMediaKit
- ✅ 可以通过浏览器观看（HTTP-FLV/WebRTC）
- ✅ 断线自动重连

---

## ⚠️ 注意事项

### 1. 密码安全

```cpp
// ❌ 错误：明文存储
camera.password = "123456";

// ✅ 正确：加密存储
camera.password = EncryptPassword("123456", encryption_key_);
```

### 2. 资源释放

```cpp
// ❌ 错误：忘记释放 FFmpeg 资源
StreamSession::~StreamSession() {
    // 空实现
}

// ✅ 正确：确保释放所有资源
StreamSession::~StreamSession() {
    Stop();
    if (demuxer_) {
        demuxer_->Close();
        demuxer_.reset();
    }
}
```

### 3. 线程安全

```cpp
// ❌ 错误：未加锁
void CameraManager::Register(const CameraInfo& camera) {
    cameras_[camera.uuid] = std::make_shared<CameraDevice>(camera);
}

// ✅ 正确：加锁保护
void CameraManager::Register(const CameraInfo& camera) {
    std::lock_guard<std::mutex> lock(mutex_);
    cameras_[camera.uuid] = std::make_shared<CameraDevice>(camera);
}
```

### 4. 异常处理

```cpp
// ❌ 错误：不处理异常
bool CameraDevice::Start() {
    http_client_->GetDeviceInfo(response);  // 可能抛出异常
    return true;
}

// ✅ 正确：捕获异常
bool CameraDevice::Start() {
    try {
        http_client_->GetDeviceInfo(response);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to get device info: {}", e.what());
        return false;
    }
}
```

---

## 📚 参考资料

- [Camera Module Design Document](CAMERA_MODULE_DESIGN.md)
- [SQLite 官方文档](https://www.sqlite.org/docs.html)
- [FFmpeg 官方文档](https://ffmpeg.org/documentation.html)
- [ONVIF 规范](https://www.onvif.org/specs/)
- [GB/T 28181 标准文档](https://github.com/ireader/media-server)
- [Boost.Asio 教程](https://www.boost.org/doc/libs/release/doc/html/boost_asio.html)

---

**最后更新：** 2026-03-28  
**维护者：** Development Team
