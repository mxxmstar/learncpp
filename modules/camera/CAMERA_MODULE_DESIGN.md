# Camera 模块详细设计文档

## 📋 目录

- [1. 模块概述](#1-模块概述)
- [2. 架构设计](#2-架构设计)
- [3. 核心组件详解](#3-核心组件详解)
- [4. 数据模型](#4-数据模型)
- [5. 数据库设计](#5-数据库设计)
- [6. 注册流程设计](#6-注册流程设计)
- [7. 协议支持规划](#7-协议支持规划)
- [8. API 接口设计](#8-api-接口设计)
- [9. 实现路线图](#9-实现路线图)
- [10. 依赖关系](#10-依赖关系)

---

## 1. 模块概述

### 1.1 模块职责

Camera 模块负责**摄像头的统一管理**，包括：

- ✅ **摄像头注册与管理** - 支持多种协议（ONVIF、GB/T 28181、手动添加）
- ✅ **远程控制中心** - 通过 HTTP API 控制摄像头参数
- ✅ **流媒体管理** - RTSP 拉流、推流到 ZLMediaKit
- ✅ **状态监控** - 实时监控摄像头在线/离线状态
- ✅ **云台控制** - PTZ 控制、变焦、截图等

### 1.2 设计原则

1. **分层架构** - 底层协议抽象，上层业务逻辑解耦
2. **协议无关** - 支持多种协议接入，易于扩展
3. **异步非阻塞** - 所有网络操作异步执行
4. **容错性强** - 自动重连、超时保护、优雅降级
5. **配置驱动** - 所有参数可通过配置文件调整

---

## 2. 架构设计

### 2.1 整体架构图

```
┌─────────────────────────────────────────────────────────┐
│                   Application Layer                      │
│              (Web API / Stream Manager)                  │
└────────────────────┬────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────┐
│                 CameraManager                            │
│          (摄像头生命周期管理 + 状态监控)                  │
└──┬──────────────┬──────────────┬────────────────────────┘
   │              │              │
┌──▼──────┐  ┌───▼──────┐  ┌──▼──────────┐
│Camera   │  │Camera    │  │Stream       │
│Device   │  │HttpClient│  │Manager      │
│(设备抽象)│  │(HTTP控制)│  │(流媒体管理)  │
└──┬──────┘  └───┬──────┘  └──┬──────────┘
   │             │            │
   │        ┌────▼────────┐  │
   │        │Protocol     │  │
   │        │Adapter      │  │
   │        │(协议适配器)  │  │
   │        └─┬────┬─────┬┘  │
   │          │    │     │    │
   │     ┌────▼┐ ┌▼───┐ ┌▼──┐│
   │     │ONVIF│ │GB/T│ │HTTP││
   │     │     │ │28181│ │API││
   │     └─────┘ └────┘ └───┘│
   │                          │
   └──────────────────────────┘
           RTSP Stream
                │
         ┌──────▼──────┐
         │ ZLMediaKit  │
         │  (流代理)    │
         └─────────────┘
```

### 2.2 模块划分

| 模块 | 文件 | 职责 |
|------|------|------|
| **CameraInfo** | `camera.h` | 摄像头数据结构定义 |
| **CameraStorage** | `camera_storage.h/cpp` | 数据库持久化（SQLite） |
| **CameraManager** | `camera_manager.h/cpp` | 摄像头生命周期管理 |
| **CameraDevice** | `camera_manager.h/cpp` | 单个摄像头设备抽象 |
| **CameraHttpClient** | `camera_httpclient.h/cpp` | HTTP API 调用封装 |
| **StreamManager** | `camera_stream.h/cpp` | 流媒体会话管理 |
| **StreamSession** | `camera_stream.h/cpp` | 单个拉流会话 |
| **ProtocolAdapter** | （待实现） | 协议适配器基类 |

---

## 3. 核心组件详解

### 3.1 CameraInfo - 摄像头信息结构体

**文件：** `include/camera/camera.h`

```cpp
struct CameraInfo {
    // ==================== 基本信息 ====================
    std::string uuid;               // 唯一标识符（主键）
    std::string name;               // 摄像头名称
    std::string vendor;             // 厂商（海康、大华、宇视等）
    std::string hardware;           // 硬件型号
    std::string software;           // 固件版本
    std::string serialnumber;       // 序列号
    std::string customer;           // 客户名称
    std::string metadata;           // JSON 格式的扩展元数据
    
    // ==================== 连接信息 ====================
    std::string rtsp_url;           // RTSP 地址（主要）
    std::string username;           // 用户名
    std::string password;           // 密码（加密存储）
    
    // ==================== 协议配置 ====================
    std::string protocol_type;      // 协议类型: "onvif", "gb28181", "http_api", "manual"
    std::string http_base_url;      // HTTP API 基础 URL（可选）
    std::string onvif_device_url;   // ONVIF 设备 URL（可选）
    std::string gb28181_id;         // GB/T 28181 设备 ID（可选）
    
    // ==================== 视频参数 ====================
    int width = 1920;               // 分辨率宽度
    int height = 1080;              // 分辨率高度
    int fps = 25;                   // 帧率
    int bitrate = 4096;             // 码率（kbps）
    
    // ==================== 状态信息 ====================
    CameraStatus status;            // 状态: Offline/Online/Streaming
    std::string create_time;        // 创建时间（ISO 8601）
    std::string update_time;        // 更新时间（ISO 8601）
    std::string last_online_time;   // 最后在线时间
    int offline_count = 0;          // 离线次数统计
};
```

**字段说明：**

| 字段组 | 用途 | 必填 |
|--------|------|------|
| 基本信息 | 设备识别和管理 | ✅ uuid, name |
| 连接信息 | RTSP 拉流 | ✅ rtsp_url |
| 协议配置 | 远程控制能力 | ❌ 根据协议类型选择 |
| 视频参数 | 默认参数配置 | ❌ 有默认值 |
| 状态信息 | 运行时状态 | ✅ 自动维护 |

---

### 3.2 CameraStorage - 数据库持久化层

**文件：** `include/camera/camera_storage.h`

#### 3.2.1 核心功能

```cpp
class CameraStorage {
public:
    // ==================== 初始化 ====================
    static CameraStorage& GetInstance();
    bool Init(const std::string& db_path);  // SQLite 数据库路径
    void Shutdown();
    
    // ==================== CRUD 操作 ====================
    bool Add(const CameraInfo& camera);                    // 新增摄像头
    bool Remove(const std::string& uuid);                  // 删除摄像头
    bool Update(const CameraInfo& camera);                 // 更新摄像头
    bool Get(const std::string& uuid, CameraInfo& camera); // 查询单个
    bool GetAll(std::vector<CameraInfo>& cameras);         // 查询全部
    
    // ==================== 高级查询 ====================
    bool GetByStatus(CameraStatus status, std::vector<CameraInfo>& cameras);
    bool GetByVendor(const std::string& vendor, std::vector<CameraInfo>& cameras);
    bool GetByProtocol(const std::string& protocol_type, std::vector<CameraInfo>& cameras);
    
    // ==================== 状态管理 ====================
    bool UpdateStatus(const std::string& uuid, CameraStatus status);
    bool UpdateLastOnlineTime(const std::string& uuid);
    bool IncrementOfflineCount(const std::string& uuid);
    
    // ==================== 批量操作 ====================
    bool BatchAdd(const std::vector<CameraInfo>& cameras);
    bool BatchUpdateStatus(const std::map<std::string, CameraStatus>& status_map);
};
```

#### 3.2.2 数据库表结构

```sql
CREATE TABLE IF NOT EXISTS cameras (
    -- 主键
    uuid TEXT PRIMARY KEY,
    
    -- 基本信息
    name TEXT NOT NULL,
    vendor TEXT,
    hardware TEXT,
    software TEXT,
    serialnumber TEXT UNIQUE,
    customer TEXT,
    metadata TEXT,  -- JSON 字符串
    
    -- 连接信息
    rtsp_url TEXT NOT NULL,
    username TEXT,
    password TEXT,  -- 加密存储
    
    -- 协议配置
    protocol_type TEXT DEFAULT 'manual',  -- onvif/gb28181/http_api/manual
    http_base_url TEXT,
    onvif_device_url TEXT,
    gb28181_id TEXT,
    
    -- 视频参数
    width INTEGER DEFAULT 1920,
    height INTEGER DEFAULT 1080,
    fps INTEGER DEFAULT 25,
    bitrate INTEGER DEFAULT 4096,
    
    -- 状态信息
    status INTEGER DEFAULT 0,  -- 0=Offline, 1=Online, 2=Streaming
    create_time TEXT NOT NULL,
    update_time TEXT NOT NULL,
    last_online_time TEXT,
    offline_count INTEGER DEFAULT 0,
    
    -- 索引
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 创建索引
CREATE INDEX IF NOT EXISTS idx_status ON cameras(status);
CREATE INDEX IF NOT EXISTS idx_vendor ON cameras(vendor);
CREATE INDEX IF NOT EXISTS idx_protocol ON cameras(protocol_type);
CREATE INDEX IF NOT EXISTS idx_customer ON cameras(customer);
```

#### 3.2.3 密码加密方案

**问题：** 数据库中不能明文存储密码

**解决方案：**
```cpp
// 使用 AES-256-CBC 加密
std::string EncryptPassword(const std::string& password, const std::string& key);
std::string DecryptPassword(const std::string& encrypted_password, const std::string& key);

// 密钥从配置文件读取
std::string encryption_key_ = ConfigManager::getInstance().get<std::string>("camera.encryption_key");
```

---

### 3.3 CameraManager - 摄像头管理器

**文件：** `include/camera/camera_manager.h`

#### 3.3.1 核心功能

```cpp
class CameraManager {
public:
    using StatusCallback = std::function<void(const std::string& uuid, CameraStatus status)>;
    
    static CameraManager& GetInstance();
    
    // ==================== 初始化 ====================
    bool Init();  // 从数据库加载所有摄像头
    void Shutdown();
    
    // ==================== 注册/注销 ====================
    bool Register(const CameraInfo& camera);      // 注册新摄像头
    bool Unregister(const std::string& uuid);     // 注销摄像头
    
    // ==================== 设备管理 ====================
    std::shared_ptr<CameraDevice> GetCamera(const std::string& uuid);
    std::vector<std::shared_ptr<CameraDevice>> GetAllCameras();
    std::vector<std::shared_ptr<CameraDevice>> GetCamerasByStatus(CameraStatus status);
    
    // ==================== 批量操作 ====================
    bool StartAllCameras();
    bool StopAllCameras();
    bool RestartFailedCameras();  // 重启失败的摄像头
    
    // ==================== 状态监控 ====================
    void SetStatusCallback(StatusCallback callback);
    void StartHealthCheck();  // 启动健康检查线程
    void StopHealthCheck();
    
private:
    std::map<std::string, std::shared_ptr<CameraDevice>> cameras_;
    StatusCallback status_callback_;
    std::unique_ptr<std::thread> health_check_thread_;
    std::atomic<bool> running_{false};
    mutable std::mutex mutex_;
};
```

#### 3.3.2 健康检查机制

```cpp
void CameraManager::HealthCheckLoop() {
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(30));
        
        for (auto& [uuid, camera] : cameras_) {
            if (camera->GetStatus() == CameraStatus::Streaming) {
                // 检查流是否正常
                if (!camera->IsStreamHealthy()) {
                    LOG_WARN("Camera {} stream unhealthy, restarting...", uuid);
                    camera->Stop();
                    camera->Start();
                }
            } else if (camera->GetStatus() == CameraStatus::Online) {
                // 检查心跳
                if (!camera->Ping()) {
                    camera->UpdateStatus(CameraStatus::Offline);
                    NotifyStatusChange(uuid, CameraStatus::Offline);
                }
            }
        }
    }
}
```

---

### 3.4 CameraDevice - 摄像头设备抽象

**文件：** `include/camera/camera_manager.h`

#### 3.4.1 核心功能

```cpp
class CameraDevice {
public:
    explicit CameraDevice(const CameraInfo& camera);
    ~CameraDevice();
    
    // ==================== 基本信息 ====================
    const CameraInfo& GetInfo() const { return camera_; }
    CameraStatus GetStatus() const { return status_; }
    std::string GetUuid() const { return camera_.uuid; }
    
    // ==================== 生命周期 ====================
    bool Start();   // 启动摄像头（连接 + 开始拉流）
    bool Stop();    // 停止摄像头
    bool Restart(); // 重启摄像头
    
    // ==================== 远程控制（HTTP API）====================
    // 视频参数
    bool SetResolution(int width, int height);
    bool SetFPS(int fps);
    bool SetBitrate(int bitrate);
    bool SetVideoParams(int width, int height, int fps, int bitrate);
    
    // 网络参数
    bool SetIPAddress(const std::string& ip, const std::string& gateway);
    bool Reboot();
    bool ResetFactory();
    
    // 云台控制
    bool PTZMove(int direction, int speed);  // direction: 0-7 (上/下/左/右/左上/右上/左下/右下)
    bool PTZStop();
    bool ZoomIn();
    bool ZoomOut();
    bool ZoomStop();
    
    // 截图
    bool CaptureSnapshot(const std::string& output_path);
    
    // ==================== 流管理 ====================
    bool StartStream();   // 开始拉流
    bool StopStream();    // 停止拉流
    bool IsStreaming() const;
    
    // ==================== 健康检查 ====================
    bool Ping();              // 心跳检测
    bool IsStreamHealthy();   // 流健康检查
    
private:
    // 内部方法
    bool InitializeProtocol();  // 初始化协议适配器
    bool SendCommand(const std::string& api, const boost::json::object& params);
    void UpdateStatus(CameraStatus status);
    void NotifyStatusChange();
    
    CameraInfo camera_;
    CameraStatus status_ = CameraStatus::Offline;
    
    // 组件
    std::unique_ptr<CameraHttpClient> http_client_;      // HTTP 客户端
    std::unique_ptr<ProtocolAdapter> protocol_adapter_;  // 协议适配器
    std::unique_ptr<StreamSession> stream_session_;      // 流会话
    
    std::mutex mutex_;
};
```

#### 3.4.2 启动流程

```cpp
bool CameraDevice::Start() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (status_ != CameraStatus::Offline) {
        LOG_WARN("Camera {} already started", camera_.uuid);
        return true;
    }
    
    try {
        // 1. 初始化协议适配器
        if (!InitializeProtocol()) {
            LOG_ERROR("Failed to initialize protocol for camera {}", camera_.uuid);
            return false;
        }
        
        // 2. 建立 HTTP 连接（如果支持）
        if (!camera_.http_base_url.empty()) {
            http_client_ = std::make_unique<CameraHttpClient>(
                camera_.http_base_url,
                camera_.username,
                camera_.password
            );
            
            if (!http_client_->GetDeviceInfo(/* response */)) {
                LOG_WARN("HTTP API not available for camera {}", camera_.uuid);
                http_client_.reset();
            }
        }
        
        // 3. 更新状态为 Online
        UpdateStatus(CameraStatus::Online);
        
        // 4. 自动开始拉流（如果配置了）
        if (ConfigManager::getInstance().get<bool>("camera.auto_start_stream", true)) {
            StartStream();
        }
        
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Camera {} start failed: {}", camera_.uuid, e.what());
        UpdateStatus(CameraStatus::Offline);
        return false;
    }
}
```

---

### 3.5 CameraHttpClient - HTTP API 客户端

**文件：** `include/camera/camera_httpclient.h`

#### 3.5.1 核心功能

```cpp
class CameraHttpClient {
public:
    explicit CameraHttpClient(const std::string& base_url, 
                             const std::string& username = "",
                             const std::string& password = "");
    
    // ==================== 通用请求 ====================
    bool Get(const std::string& api, boost::json::object& response);
    bool Post(const std::string& api, const boost::json::object& params, 
              boost::json::object& response);
    bool Put(const std::string& api, const boost::json::object& params,
             boost::json::object& response);
    
    // ==================== 设备信息 ====================
    bool GetDeviceInfo(boost::json::object& info);
    bool GetNetworkInfo(boost::json::object& info);
    bool GetVideoParams(boost::json::object& params);
    
    // ==================== 参数设置 ====================
    bool SetVideoParams(int width, int height, int fps, int bitrate);
    bool SetNetworkParams(const std::string& ip, const std::string& gateway, 
                         const std::string& dns);
    bool SetDateTime(const std::string& datetime);
    
    // ==================== 设备控制 ====================
    bool RebootDevice();
    bool ResetFactory();
    bool UpgradeFirmware(const std::string& firmware_url);
    
    // ==================== 云台控制 ====================
    bool PTZMove(int direction, int speed);  // 0-7 方向
    bool PTZZoom(bool zoom_in);
    bool PTZFocusing(bool focus_near);
    
    // ==================== 图像参数 ====================
    bool SetBrightness(int value);  // 0-255
    bool SetContrast(int value);    // 0-255
    bool SetSaturation(int value);  // 0-255
    bool SetSharpness(int value);   // 0-255
    
    // ==================== 截图 ====================
    bool GetSnapshot(const std::string& save_path);
    bool GetSnapshotBase64(std::string& base64_data);
    
private:
    // 内部方法
    std::string BuildUrl(const std::string& api);
    bool ParseResponse(const boost::json::object& response, int& code, std::string& msg);
    std::string BuildAuthHeader();  // Basic Auth / Digest Auth
    
    std::string base_url_;
    std::string username_;
    std::string password_;
    std::shared_ptr<Net::PooledClient> http_client_;  // 使用连接池
    
    // 厂商特定的 API 路径映射
    std::map<std::string, std::string> api_paths_;  // vendor -> api_path
};
```

#### 3.5.2 厂商适配示例

```cpp
// 海康威视 API 路径
if (camera_.vendor == "hikvision") {
    api_paths_ = {
        {"device_info", "/ISAPI/System/deviceInfo"},
        {"video_params", "/ISAPI/Image/channels/1"},
        {"ptz_control", "/ISAPI/PTZCtrl/channels/1/continuous"},
        {"snapshot", "/ISAPI/Streaming/channels/1/picture"}
    };
}
// 大华 API 路径
else if (camera_.vendor == "dahua") {
    api_paths_ = {
        {"device_info", "/cgi-bin/magicBox.cgi?action=getSystemInfo"},
        {"video_params", "/cgi-bin/configManager.cgi?action=getConfig&name=Encode[0].MainFormat"},
        {"ptz_control", "/cgi-bin/ptz.cgi?action=start&channel=0"},
        {"snapshot", "/cgi-bin/snapshot.cgi?channel=1"}
    };
}
```

---

### 3.6 StreamManager & StreamSession - 流媒体管理

**文件：** `include/camera/camera_stream.h`

#### 3.6.1 StreamManager

```cpp
class StreamManager {
public:
    static StreamManager& GetInstance();
    
    bool Init();
    void Shutdown();
    
    // ==================== 流控制 ====================
    bool StartStream(const CameraInfo& camera);
    bool StopStream(const std::string& camera_uuid);
    bool RestartStream(const std::string& camera_uuid);
    
    // ==================== 状态查询 ====================
    bool IsStreaming(const std::string& camera_uuid);
    StreamSession* GetSession(const std::string& camera_uuid);
    std::vector<std::string> GetAllStreamingCameras();
    
    // ==================== 统计信息 ====================
    struct StreamStats {
        uint64_t total_bytes = 0;
        uint64_t total_frames = 0;
        double current_bitrate_kbps = 0.0;
        int64_t start_time = 0;  // Unix timestamp
        int reconnect_count = 0;
    };
    StreamStats GetStats(const std::string& camera_uuid);
    
private:
    std::map<std::string, std::unique_ptr<StreamSession>> sessions_;
    mutable std::mutex mutex_;
};
```

#### 3.6.2 StreamSession

```cpp
class StreamSession {
public:
    explicit StreamSession(const CameraInfo& camera);
    ~StreamSession();
    
    bool Start();   // 开始拉流
    bool Stop();    // 停止拉流
    bool Restart(); // 重启拉流
    
    const CameraInfo& GetCamera() const { return camera_; }
    bool IsRunning() const { return running_; }
    StreamManager::StreamStats GetStats() const;
    
private:
    // FFmpeg 回调
    void OnDataReceived(const uint8_t* data, size_t size);
    void OnError(const std::string& error);
    void OnReconnect();
    
    // 统计更新
    void UpdateStats();
    
    CameraInfo camera_;
    bool running_ = false;
    
    // FFmpeg 解复用器
    std::unique_ptr<FFmpegDemuxer> demuxer_;
    
    // 统计信息
    uint64_t total_bytes_ = 0;
    uint64_t total_frames_ = 0;
    int64_t start_time_ = 0;
    int reconnect_count_ = 0;
    
    std::mutex mutex_;
};
```

---

## 4. 数据模型

### 4.1 CameraStatus 枚举

```cpp
enum class CameraStatus {
    Offline = 0,      // 离线
    Online = 1,       // 在线（已连接但未拉流）
    Streaming = 2     // 推流中
};
```

**状态转换图：**

```
Offline ──Start()──> Online ──StartStream()──> Streaming
   ^                    |                           |
   |                    |──StopStream()             |──StopStream()
   |                    v                           v
   └────Stop()──────── Offline <────Error───────── Streaming
```

---

## 5. 数据库设计

### 5.1 完整 SQL 脚本

```sql
-- 摄像头表
CREATE TABLE IF NOT EXISTS cameras (
    uuid TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    vendor TEXT,
    hardware TEXT,
    software TEXT,
    serialnumber TEXT UNIQUE,
    customer TEXT,
    metadata TEXT,
    
    rtsp_url TEXT NOT NULL,
    username TEXT,
    password TEXT,
    
    protocol_type TEXT DEFAULT 'manual',
    http_base_url TEXT,
    onvif_device_url TEXT,
    gb28181_id TEXT,
    
    width INTEGER DEFAULT 1920,
    height INTEGER DEFAULT 1080,
    fps INTEGER DEFAULT 25,
    bitrate INTEGER DEFAULT 4096,
    
    status INTEGER DEFAULT 0,
    create_time TEXT NOT NULL,
    update_time TEXT NOT NULL,
    last_online_time TEXT,
    offline_count INTEGER DEFAULT 0,
    
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 索引
CREATE INDEX IF NOT EXISTS idx_status ON cameras(status);
CREATE INDEX IF NOT EXISTS idx_vendor ON cameras(vendor);
CREATE INDEX IF NOT EXISTS idx_protocol ON cameras(protocol_type);
CREATE INDEX IF NOT EXISTS idx_customer ON cameras(customer);
CREATE INDEX IF NOT EXISTS idx_serialnumber ON cameras(serialnumber);

-- 触发器：自动更新 update_time
CREATE TRIGGER IF NOT EXISTS update_cameras_timestamp 
AFTER UPDATE ON cameras
BEGIN
    UPDATE cameras SET update_time = datetime('now') WHERE uuid = OLD.uuid;
END;
```

---

## 6. 注册流程设计

### 6.1 注册方式对比

| 注册方式 | 适用场景 | 自动化程度 | 实现难度 |
|---------|---------|-----------|---------|
| **手动添加** | 已知 RTSP URL | ⭐⭐ | 简单 |
| **ONVIF 发现** | 局域网内 ONVIF 摄像头 | ⭐⭐⭐⭐ | 中等 |
| **GB/T 28181** | 国标平台接入 | ⭐⭐⭐⭐⭐ | 复杂 |
| **HTTP API 扫描** | 支持 HTTP API 的摄像头 | ⭐⭐⭐ | 中等 |

---

### 6.2 手动添加流程

```
用户输入摄像头信息
    ↓
前端表单提交 (POST /api/camera/add)
    ↓
后端验证参数
    ↓
生成 UUID
    ↓
加密密码
    ↓
存入数据库 (CameraStorage::Add)
    ↓
创建 CameraDevice 对象
    ↓
添加到 CameraManager
    ↓
自动启动（如果配置了 auto_start）
    ↓
返回成功
```

**API 请求示例：**

```json
POST /api/camera/add
{
    "name": "门口摄像头",
    "vendor": "hikvision",
    "rtsp_url": "rtsp://admin:123456@192.168.1.100:554/stream",
    "username": "admin",
    "password": "123456",
    "protocol_type": "manual",
    "width": 1920,
    "height": 1080,
    "fps": 25
}
```

---

### 6.3 ONVIF 自动发现流程

```
用户点击"扫描局域网"
    ↓
发送 WS-Discovery 多播消息
    ↓
接收 ONVIF 设备响应
    ↓
解析设备信息（UUID、名称、URL）
    ↓
尝试获取 RTSP URL（通过 ONVIF Media Service）
    ↓
显示发现的摄像头列表
    ↓
用户选择要添加的摄像头
    ↓
输入用户名密码
    ↓
保存到数据库
    ↓
启动摄像头
```

**技术要点：**

1. **WS-Discovery 协议** - UDP 多播发现
2. **ONVIF Media Service** - 获取 Profile 和 RTSP URL
3. **ONVIF PTZ Service** - 云台控制能力查询

**需要实现的类：**

```cpp
class OnvifDiscovery {
public:
    struct DiscoveredDevice {
        std::string uuid;
        std::string name;
        std::string manufacturer;
        std::string model;
        std::string device_url;  // ONVIF 设备 URL
        std::vector<std::string> rtsp_urls;
    };
    
    std::vector<DiscoveredDevice> Discover(int timeout_sec = 5);
    bool GetRTSPUrls(const std::string& device_url, 
                    const std::string& username,
                    const std::string& password,
                    std::vector<std::string>& rtsp_urls);
};
```

---

### 6.4 GB/T 28181 注册流程

```
GB/T 28181 SIP 服务器启动
    ↓
摄像头主动注册（SIP REGISTER）
    ↓
验证设备 ID 和密码
    ↓
回复 200 OK
    ↓
保存设备信息到数据库
    ↓
订阅目录（SIP SUBSCRIBE）
    ↓
接收设备状态通知
    ↓
按需邀请视频流（SIP INVITE）
    ↓
接收 RTP 流
    ↓
转发到 ZLMediaKit
```

**技术要点：**

1. **SIP 协议栈** - 需要使用 PJSIP 或 osip2
2. **SDP 协商** - 媒体参数协商
3. **RTP/RTCP** - 实时传输协议
4. **PS 流解复用** - GB/T 28181 使用 PS 封装

**需要实现的类：**

```cpp
class GB28181Server {
public:
    bool Init(const std::string& sip_id, int sip_port);
    void Shutdown();
    
    // SIP 消息处理
    void OnRegister(const SipMessage& msg);
    void OnInvite(const SipMessage& msg);
    void OnNotify(const SipMessage& msg);
    
    // 设备管理
    std::vector<GB28181Device> GetDevices();
    bool StartStream(const std::string& device_id);
    bool StopStream(const std::string& device_id);
};

struct GB28181Device {
    std::string device_id;      // 20 位国标 ID
    std::string name;
    std::string manufacturer;
    std::string status;         // Online/Offline
    std::string register_time;
    std::string last_keepalive;
};
```

---

## 7. 协议支持规划

### 7.1 协议适配器架构

```cpp
// 协议适配器基类
class ProtocolAdapter {
public:
    virtual ~ProtocolAdapter() = default;
    
    virtual bool Initialize(const CameraInfo& camera) = 0;
    virtual bool Cleanup() = 0;
    
    // 设备信息查询
    virtual bool GetDeviceInfo(boost::json::object& info) = 0;
    
    // 参数设置
    virtual bool SetVideoParams(int width, int height, int fps) = 0;
    virtual bool SetNetworkParams(const std::string& ip) = 0;
    
    // 云台控制
    virtual bool PTZMove(int direction, int speed) = 0;
    virtual bool PTZZoom(bool zoom_in) = 0;
    
    // 截图
    virtual bool CaptureSnapshot(const std::string& output_path) = 0;
    
    // 设备控制
    virtual bool Reboot() = 0;
    virtual bool ResetFactory() = 0;
    
    // 健康检查
    virtual bool Ping() = 0;
    
    virtual std::string GetProtocolName() const = 0;
};

// ONVIF 适配器
class OnvifAdapter : public ProtocolAdapter {
    // 实现 ONVIF SOAP 协议
};

// GB/T 28181 适配器
class GB28181Adapter : public ProtocolAdapter {
    // 实现 SIP 协议
};

// HTTP API 适配器
class HttpApiAdapter : public ProtocolAdapter {
    // 使用 CameraHttpClient
};

// 手动模式（仅 RTSP 拉流，无控制能力）
class ManualAdapter : public ProtocolAdapter {
    // 最小实现
};
```

### 7.2 协议能力矩阵

| 功能 | ONVIF | GB/T 28181 | HTTP API | Manual |
|------|-------|------------|----------|--------|
| **RTSP 拉流** | ✅ | ✅ | ✅ | ✅ |
| **设备信息查询** | ✅ | ⚠️ | ✅ | ❌ |
| **视频参数设置** | ✅ | ❌ | ✅ | ❌ |
| **云台控制** | ✅ | ⚠️ | ✅ | ❌ |
| **截图** | ✅ | ❌ | ✅ | ❌ |
| **重启设备** | ✅ | ❌ | ✅ | ❌ |
| **自动发现** | ✅ | ❌ | ❌ | ❌ |
| **跨网段** | ❌ | ✅ | ⚠️ | ✅ |

---

## 8. API 接口设计

### 8.1 RESTful API

#### 8.1.1 摄像头管理

```
GET    /api/cameras              # 获取所有摄像头列表
GET    /api/cameras/{uuid}       # 获取单个摄像头详情
POST   /api/cameras              # 添加摄像头
PUT    /api/cameras/{uuid}       # 更新摄像头信息
DELETE /api/cameras/{uuid}       # 删除摄像头

POST   /api/cameras/discover     # ONVIF 自动发现
POST   /api/cameras/batch_add    # 批量添加
```

#### 8.1.2 摄像头控制

```
POST   /api/cameras/{uuid}/start       # 启动摄像头
POST   /api/cameras/{uuid}/stop        # 停止摄像头
POST   /api/cameras/{uuid}/restart     # 重启摄像头

POST   /api/cameras/{uuid}/stream/start   # 开始拉流
POST   /api/cameras/{uuid}/stream/stop    # 停止拉流

POST   /api/cameras/{uuid}/ptz/move       # 云台移动
POST   /api/cameras/{uuid}/ptz/zoom       # 变焦
POST   /api/cameras/{uuid}/snapshot       # 截图

PUT    /api/cameras/{uuid}/params/video   # 设置视频参数
PUT    /api/cameras/{uuid}/params/network # 设置网络参数

POST   /api/cameras/{uuid}/reboot         # 重启设备
POST   /api/cameras/{uuid}/reset          # 恢复出厂
```

#### 8.1.3 状态查询

```
GET    /api/cameras/{uuid}/status      # 获取状态
GET    /api/cameras/{uuid}/stats       # 获取统计信息
GET    /api/cameras/{uuid}/health      # 健康检查
```

---

### 8.2 WebSocket 实时推送

```
ws://localhost:8081/ws/camera/events

事件类型：
- camera.status_changed  # 状态变化
- camera.stream_started  # 开始推流
- camera.stream_stopped  # 停止推流
- camera.error           # 错误事件
```

**消息格式：**

```json
{
    "event": "camera.status_changed",
    "timestamp": "2026-03-28T10:30:00Z",
    "data": {
        "uuid": "cam_001",
        "old_status": "offline",
        "new_status": "online"
    }
}
```

---

## 9. 实现路线图

### Phase 1: 基础框架（2 周）

**目标：** 完成核心数据结构和数据库持久化

- [ ] 完善 `CameraInfo` 结构体
- [ ] 实现 `CameraStorage`（SQLite CRUD）
- [ ] 实现密码加密/解密
- [ ] 编写单元测试
- [ ] 数据库迁移脚本

**交付物：**
- ✅ 可以手动添加/删除摄像头
- ✅ 数据持久化到 SQLite
- ✅ 基本的查询功能

---

### Phase 2: CameraManager 和 CameraDevice（2 周）

**目标：** 实现摄像头生命周期管理

- [ ] 实现 `CameraManager` 单例
- [ ] 实现 `CameraDevice` 基础功能
- [ ] 实现状态机（Offline → Online → Streaming）
- [ ] 实现健康检查线程
- [ ] 集成 `StreamManager`

**交付物：**
- ✅ 可以从数据库加载所有摄像头
- ✅ 可以启动/停止摄像头
- ✅ 自动健康检查和重连

---

### Phase 3: CameraHttpClient（1 周）

**目标：** 实现 HTTP API 调用

- [ ] 实现通用 HTTP 请求（GET/POST/PUT）
- [ ] 实现认证（Basic Auth / Digest Auth）
- [ ] 实现海康威视 API 适配
- [ ] 实现大华 API 适配
- [ ] 编写 API 测试用例

**交付物：**
- ✅ 可以远程控制海康/大华摄像头
- ✅ 支持视频参数设置
- ✅ 支持云台控制
- ✅ 支持截图

---

### Phase 4: StreamManager（1 周）

**目标：** 实现 RTSP 拉流管理

- [ ] 实现 `StreamSession`（FFmpeg 解复用）
- [ ] 实现 `StreamManager` 单例
- [ ] 实现流统计（码率、帧数、字节数）
- [ ] 实现自动重连
- [ ] 集成到 ZLMediaKit

**交付物：**
- ✅ 可以拉取 RTSP 流
- ✅ 推送到 ZLMediaKit
- ✅ 实时监控流状态
- ✅ 断线自动重连

---

### Phase 5: ONVIF 支持（2 周）

**目标：** 实现 ONVIF 协议

- [ ] 实现 WS-Discovery 发现
- [ ] 实现 ONVIF Device Service
- [ ] 实现 ONVIF Media Service（获取 RTSP URL）
- [ ] 实现 ONVIF PTZ Service
- [ ] 实现 ONVIF Imaging Service

**依赖库：** gSOAP 或 libonvif

**交付物：**
- ✅ 可以自动发现局域网 ONVIF 摄像头
- ✅ 可以获取 RTSP URL
- ✅ 支持云台控制
- ✅ 支持截图

---

### Phase 6: GB/T 28181 支持（4 周）

**目标：** 实现国标协议

- [ ] 集成 PJSIP 或 osip2
- [ ] 实现 SIP REGISTER
- [ ] 实现 SIP INVITE/ACK/BYE
- [ ] 实现 SDP 协商
- [ ] 实现 RTP/RTCP 接收
- [ ] 实现 PS 流解复用
- [ ] 实现目录订阅

**依赖库：** PJSIP + FFmpeg

**交付物：**
- ✅ 可以作为 SIP 服务器接收注册
- ✅ 可以邀请视频流
- ✅ 可以接收 RTP 流
- ✅ 转发到 ZLMediaKit

---

### Phase 7: Web API 集成（1 周）

**目标：** 提供完整的 RESTful API

- [ ] 实现 `/api/cameras/*` 路由
- [ ] 实现 `/api/cameras/{uuid}/control/*` 路由
- [ ] 实现 WebSocket 事件推送
- [ ] 编写 API 文档（Swagger/OpenAPI）
- [ ] 前端联调

**交付物：**
- ✅ 完整的 RESTful API
- ✅ WebSocket 实时推送
- ✅ API 文档
- ✅ 前端可以正常调用

---

### Phase 8: 优化和完善（2 周）

**目标：** 性能优化和异常处理

- [ ] 压力测试（100+ 摄像头）
- [ ] 内存泄漏检测
- [ ] 异常场景处理（网络断开、设备重启等）
- [ ] 日志完善
- [ ] 监控指标（Prometheus）
- [ ] 配置文件优化

**交付物：**
- ✅ 稳定运行 7x24 小时
- ✅ 支持 100+ 摄像头并发
- ✅ 完善的监控和告警

---

## 10. 依赖关系

### 10.1 第三方库依赖

| 库 | 版本 | 用途 | CMake Target |
|----|------|------|--------------|
| **SQLite3** | 3.x | 数据库持久化 | `SQLite::SQLite3` |
| **Boost.Asio** | 1.90 | 网络通信 | `Boost::asio` |
| **Boost.Json** | 1.90 | JSON 解析 | `Boost::json` |
| **FFmpeg** | 6.x | RTSP 拉流、解码 | `avcodec`, `avformat`, `avutil` |
| **gSOAP** | 2.8+ | ONVIF SOAP（可选） | `gsoap` |
| **PJSIP** | 2.x | GB/T 28181 SIP（可选） | `pjproject` |
| **OpenSSL** | 3.x | 密码加密 | `OpenSSL::SSL` |

### 10.2 模块依赖

```
camera_lib
├── log_lib              # 日志
├── sqlite_lib           # 数据库
├── net_lib              # HTTP 客户端
├── config_lib           # 配置管理
├── ffmpeg_opt_lib       # FFmpeg 封装
└── zlmediakit_lib       # ZLMediaKit 集成
```

### 10.3 CMakeLists.txt 示例

```cmake
cmake_minimum_required(VERSION 3.18)
project(camera_lib LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 收集源文件
file(GLOB_RECURSE LIB_SOURCES "src/*.cpp")

# 创建库
add_library(${PROJECT_NAME} ${LIB_SOURCES})

# 包含目录
target_include_directories(${PROJECT_NAME} 
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 链接依赖
target_link_libraries(${PROJECT_NAME}
    PUBLIC
        log_lib
        sqlite_lib
        net_lib
        config_lib
    PRIVATE
        ffmpeg_opt_lib
        zlmediakit_lib
        OpenSSL::SSL
        Boost::json
)

# Windows 特定配置
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()
```

---

## 11. 测试计划

### 11.1 单元测试

```cpp
// test_camera_storage.cpp
TEST(CameraStorageTest, AddAndGet) {
    CameraStorage& storage = CameraStorage::GetInstance();
    storage.Init("./test_camera.db");
    
    CameraInfo camera;
    camera.uuid = "test_001";
    camera.name = "Test Camera";
    camera.rtsp_url = "rtsp://test/stream";
    
    EXPECT_TRUE(storage.Add(camera));
    
    CameraInfo retrieved;
    EXPECT_TRUE(storage.Get("test_001", retrieved));
    EXPECT_EQ(retrieved.name, "Test Camera");
}

// test_camera_httpclient.cpp
TEST(CameraHttpClientTest, GetDeviceInfo) {
    CameraHttpClient client("http://192.168.1.100", "admin", "123456");
    
    boost::json::object info;
    EXPECT_TRUE(client.GetDeviceInfo(info));
    EXPECT_FALSE(info.empty());
}
```

### 11.2 集成测试

```cpp
// test_camera_manager.cpp
TEST(CameraManagerTest, FullLifecycle) {
    CameraManager& manager = CameraManager::GetInstance();
    manager.Init();
    
    // 1. 注册摄像头
    CameraInfo camera;
    camera.uuid = "test_001";
    camera.rtsp_url = "rtsp://test/stream";
    EXPECT_TRUE(manager.Register(camera));
    
    // 2. 启动摄像头
    auto device = manager.GetCamera("test_001");
    EXPECT_TRUE(device->Start());
    EXPECT_EQ(device->GetStatus(), CameraStatus::Online);
    
    // 3. 开始拉流
    EXPECT_TRUE(device->StartStream());
    EXPECT_EQ(device->GetStatus(), CameraStatus::Streaming);
    
    // 4. 云台控制
    EXPECT_TRUE(device->PTZMove(0, 50));  // 向上
    
    // 5. 停止
    EXPECT_TRUE(device->StopStream());
    EXPECT_TRUE(device->Stop());
    
    // 6. 注销
    EXPECT_TRUE(manager.Unregister("test_001"));
}
```

### 11.3 压力测试

```bash
# 模拟 100 个摄像头同时拉流
./stress_test --camera-count 100 --duration 3600

# 监控指标
- CPU 使用率
- 内存使用率
- 网络带宽
- 丢包率
- 延迟
```

---

## 12. 常见问题与解决方案

### 12.1 RTSP 连接失败

**问题：** 摄像头 RTSP URL 无法连接

**排查步骤：**
1. 检查网络连接（ping 摄像头 IP）
2. 检查用户名密码是否正确
3. 检查 RTSP URL 格式（不同厂商格式不同）
4. 使用 VLC 测试 RTSP URL
5. 检查防火墙规则

**常见 RTSP URL 格式：**
```
海康威视: rtsp://admin:123456@192.168.1.100:554/h264/ch1/main/av_stream
大    华: rtsp://admin:123456@192.168.1.100:554/cam/realmonitor?channel=1&subtype=0
宇    视: rtsp://admin:123456@192.168.1.100:554/video1
```

---

### 12.2 云台控制无响应

**问题：** PTZ 控制命令发送成功但摄像头无反应

**可能原因：**
1. 摄像头不支持 PTZ
2. 权限不足（需要管理员权限）
3. API 路径错误（不同厂商不同）
4. 云台被锁定

**解决方案：**
```cpp
// 先查询 PTZ 能力
boost::json::object capabilities;
if (http_client_->GetPTZCapabilities(capabilities)) {
    if (capabilities["move"].to_bool()) {
        // 支持移动
        http_client_->PTZMove(direction, speed);
    }
}
```

---

### 12.3 内存泄漏

**问题：** 长时间运行后内存持续增长

**排查工具：**
- Windows: Visual Studio Diagnostic Tools
- Linux: Valgrind

**常见原因：**
1. StreamSession 未正确释放
2. FFmpeg 上下文未关闭
3. 循环引用（shared_ptr）

**解决方案：**
```cpp
// 确保正确释放 FFmpeg 资源
StreamSession::~StreamSession() {
    Stop();  // 先停止
    if (demuxer_) {
        demuxer_->Close();
        demuxer_.reset();
    }
}
```

---

## 13. 性能优化建议

### 13.1 连接池

```cpp
// 使用 HttpClientPool 管理多个摄像头的 HTTP 连接
class CameraConnectionPool {
    std::map<std::string, std::shared_ptr<CameraHttpClient>> pool_;
    
    std::shared_ptr<CameraHttpClient> GetClient(const std::string& uuid) {
        if (pool_.find(uuid) == pool_.end()) {
            // 创建新连接
        }
        return pool_[uuid];
    }
};
```

### 13.2 异步操作

```cpp
// 所有网络操作异步执行
bool CameraDevice::StartStreamAsync() {
    std::async(std::launch::async, [this]() {
        return StartStream();
    });
    return true;  // 立即返回
}
```

### 13.3 批量数据库操作

```cpp
// 批量更新状态
std::map<std::string, CameraStatus> status_updates;
for (auto& [uuid, camera] : cameras_) {
    status_updates[uuid] = camera->GetStatus();
}
CameraStorage::GetInstance().BatchUpdateStatus(status_updates);
```

---

## 14. 安全考虑

### 14.1 密码安全

- ✅ 数据库中使用 AES-256 加密存储
- ✅ 密钥从配置文件读取，不在代码中硬编码
- ✅ 传输层使用 HTTPS（如果支持）

### 14.2 访问控制

- ✅ API 需要身份验证（JWT Token）
- ✅ 基于角色的权限控制（RBAC）
- ✅ 敏感操作需要二次确认（重启、恢复出厂）

### 14.3 网络安全

- ✅ RTSP 流使用 SRTP（如果摄像头支持）
- ✅ ONVIF 使用 WS-Security
- ✅ GB/T 28181 使用 SIP 认证

---

## 15. 总结

Camera 模块是一个**复杂的子系统**，涉及：

1. **多种协议** - ONVIF、GB/T 28181、HTTP API、RTSP
2. **多种技术** - SQLite、FFmpeg、SIP、SOAP、HTTP
3. **多种场景** - 手动添加、自动发现、批量管理

**实施建议：**

1. **分阶段实施** - 先完成 Phase 1-4，再逐步添加 ONVIF 和 GB/T 28181
2. **充分测试** - 每个阶段都要有完整的单元测试和集成测试
3. **文档完善** - 及时更新 API 文档和使用手册
4. **监控告警** - 生产环境必须有完善的监控

**预期成果：**

- ✅ 支持 100+ 摄像头并发管理
- ✅ 支持 4 种协议接入
- ✅ 完整的 RESTful API
- ✅ 7x24 小时稳定运行
- ✅ 完善的监控和告警

---

**文档版本：** v1.0  
**最后更新：** 2026-03-28  
**作者：** AI Assistant  
**审核状态：** 待审核
