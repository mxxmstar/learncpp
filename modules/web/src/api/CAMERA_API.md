# Camera API 文档

## 📋 概述

Camera API 提供摄像头的 CRUD 操作和状态管理功能。

**基础路径：** `/camera`

**请求格式：** JSON  
**响应格式：** JSON

---

## 📡 API 列表

### 1. 添加摄像头

**接口：** `POST /camera/add`

**描述：** 注册一个新的摄像头设备

**请求参数：**

| 参数 | 类型 | 必需 | 说明 |
|------|------|------|------|
| uuid | string | ✅ | 摄像头唯一标识符 |
| name | string | ✅ | 摄像头名称 |
| rtsp_url | string | ✅ | RTSP 流地址 |
| username | string | ❌ | 用户名 |
| password | string | ❌ | 密码（明文存储） |
| vendor | string | ❌ | 厂商（hikvision/dahua等） |
| protocol_type | string | ❌ | 协议类型（manual/onvif/gb28181） |
| width | int | ❌ | 分辨率宽度（默认1920） |
| height | int | ❌ | 分辨率高度（默认1080） |

**请求示例：**

```json
{
    "uuid": "cam_001",
    "name": "大门摄像头",
    "rtsp_url": "rtsp://admin:123456@192.168.1.100:554/stream",
    "username": "admin",
    "password": "123456",
    "vendor": "hikvision",
    "protocol_type": "manual",
    "width": 1920,
    "height": 1080
}
```

**响应示例：**

```json
{
    "code": 200,
    "msg": "Camera added successfully",
    "data": {
        "uuid": "cam_001",
        "name": "大门摄像头",
        "rtsp_url": "rtsp://admin:123456@192.168.1.100:554/stream",
        "vendor": "hikvision",
        "status": "offline",
        "create_time": 1743134400
    }
}
```

---

### 2. 删除摄像头

**接口：** `POST /camera/remove`

**描述：** 删除一个摄像头设备

**请求参数：**

| 参数 | 类型 | 必需 | 说明 |
|------|------|------|------|
| uuid | string | ✅ | 摄像头唯一标识符 |

**请求示例：**

```json
{
    "uuid": "cam_001"
}
```

**响应示例：**

```json
{
    "code": 200,
    "msg": "Camera removed successfully"
}
```

---

### 3. 更新摄像头信息

**接口：** `POST /camera/update`

**描述：** 更新摄像头的配置信息

**请求参数：** 完整的 CameraInfo 对象（至少包含 uuid）

**请求示例：**

```json
{
    "uuid": "cam_001",
    "name": "更新后的名称",
    "rtsp_url": "rtsp://new_url",
    "width": 2560,
    "height": 1440
}
```

**响应示例：**

```json
{
    "code": 200,
    "msg": "Camera updated successfully",
    "data": {
        "uuid": "cam_001",
        "name": "更新后的名称",
        ...
    }
}
```

---

### 4. 获取单个摄像头信息

**接口：** `GET /camera/get`

**描述：** 查询指定摄像头的详细信息

**请求参数：**

| 参数 | 类型 | 必需 | 说明 |
|------|------|------|------|
| uuid | string | ✅ | 摄像头唯一标识符 |

**请求示例：**

```json
{
    "uuid": "cam_001"
}
```

**响应示例：**

```json
{
    "code": 200,
    "msg": "Success",
    "data": {
        "uuid": "cam_001",
        "name": "大门摄像头",
        "vendor": "hikvision",
        "rtsp_url": "rtsp://admin:123456@192.168.1.100:554/stream",
        "width": 1920,
        "height": 1080,
        "fps": 25,
        "bitrate": 4096,
        "status": "online",
        "protocol_type": "manual",
        "create_time": 1743134400,
        "update_time": 1743138000,
        "last_online_time": 1743137500
    }
}
```

---

### 5. 获取所有摄像头列表

**接口：** `GET /camera/list`

**描述：** 查询所有已注册的摄像头

**请求参数：** 无

**响应示例：**

```json
{
    "code": 200,
    "msg": "Success",
    "total": 3,
    "data": [
        {
            "uuid": "cam_001",
            "name": "大门摄像头",
            "status": "online",
            ...
        },
        {
            "uuid": "cam_002",
            "name": "后门摄像头",
            "status": "offline",
            ...
        }
    ]
}
```

---

### 6. 按状态查询摄像头

**接口：** `GET /camera/by_status`

**描述：** 查询指定状态的摄像头列表

**请求参数：**

| 参数 | 类型 | 必需 | 说明 |
|------|------|------|------|
| status | string | ✅ | 状态：offline/online/streaming |

**请求示例：**

```json
{
    "status": "online"
}
```

**响应示例：**

```json
{
    "code": 200,
    "msg": "Success",
    "total": 2,
    "status": "online",
    "data": [
        {
            "uuid": "cam_001",
            "name": "大门摄像头",
            "status": "online",
            ...
        }
    ]
}
```

---

### 7. 按厂商查询摄像头

**接口：** `GET /camera/by_vendor`

**描述：** 查询指定厂商的摄像头列表

**请求参数：**

| 参数 | 类型 | 必需 | 说明 |
|------|------|------|------|
| vendor | string | ✅ | 厂商标识 |

**请求示例：**

```json
{
    "vendor": "hikvision"
}
```

**响应示例：**

```json
{
    "code": 200,
    "msg": "Success",
    "total": 5,
    "vendor": "hikvision",
    "data": [
        {
            "uuid": "cam_001",
            "name": "大门摄像头",
            "vendor": "hikvision",
            ...
        }
    ]
}
```

---

### 8. 更新摄像头状态

**接口：** `POST /camera/update_status`

**描述：** 更新摄像头的在线状态

**请求参数：**

| 参数 | 类型 | 必需 | 说明 |
|------|------|------|------|
| uuid | string | ✅ | 摄像头唯一标识符 |
| status | string | ✅ | 新状态：offline/online/streaming |

**请求示例：**

```json
{
    "uuid": "cam_001",
    "status": "online"
}
```

**响应示例：**

```json
{
    "code": 200,
    "msg": "Camera status updated successfully",
    "data": {
        "uuid": "cam_001",
        "status": "online"
    }
}
```

---

## 🔢 响应码说明

| 响应码 | 说明 |
|--------|------|
| 200 | 成功 |
| 400 | 请求参数错误 |
| 404 | 资源不存在 |
| 500 | 服务器内部错误 |

---

## 📊 数据结构

### CameraInfo 完整结构

```json
{
    // 基本信息
    "uuid": "cam_001",
    "name": "大门摄像头",
    "vendor": "hikvision",
    "hardware": "DS-2CD3T47G2-L",
    "software": "V5.7.0",
    "serialnumber": "DS-XXXXX",
    "customer": "客户A",
    "metadata": "{}",
    "create_time": 1743134400,
    "update_time": 1743138000,
    
    // 连接信息
    "rtsp_url": "rtsp://admin:123456@192.168.1.100:554/stream",
    "username": "admin",
    "password": "123456",
    
    // 协议配置
    "protocol_type": "manual",
    "http_base_url": "",
    "onvif_device_url": "",
    "gb28181_id": "",
    
    // 视频参数
    "width": 1920,
    "height": 1080,
    "fps": 25,
    "bitrate": 4096,
    
    // 状态信息
    "status": "online",
    "last_online_time": 1743137500,
    "offline_count": 2
}
```

### 时间戳说明

所有时间字段使用 **Unix 时间戳（秒）**：

```javascript
// JavaScript 转换为可读时间
const createTime = new Date(camera.create_time * 1000).toLocaleString();
// 输出: "2026/3/28 10:30:00"
```

---

## 💡 使用示例

### cURL 示例

#### 1. 添加摄像头

```bash
curl -X POST http://localhost:8080/camera/add \
  -H "Content-Type: application/json" \
  -d '{
    "uuid": "cam_001",
    "name": "测试摄像头",
    "rtsp_url": "rtsp://admin:123456@192.168.1.100:554/stream",
    "vendor": "hikvision"
  }'
```

#### 2. 获取摄像头列表

```bash
curl -X GET http://localhost:8080/camera/list
```

#### 3. 更新摄像头状态

```bash
curl -X POST http://localhost:8080/camera/update_status \
  -H "Content-Type: application/json" \
  -d '{
    "uuid": "cam_001",
    "status": "online"
  }'
```

### JavaScript 示例

```javascript
// 添加摄像头
async function addCamera(cameraData) {
    const response = await fetch('/camera/add', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(cameraData)
    });
    return await response.json();
}

// 获取摄像头列表
async function getCameraList() {
    const response = await fetch('/camera/list');
    return await response.json();
}

// 使用示例
const result = await addCamera({
    uuid: 'cam_001',
    name: '大门摄像头',
    rtsp_url: 'rtsp://...',
    vendor: 'hikvision'
});

console.log(result);
// { code: 200, msg: "Camera added successfully", data: {...} }
```

---

## ⚠️ 注意事项

### 1. 密码安全

⚠️ **当前版本密码明文存储和传输**，仅用于开发调试。

生产环境需要：
- 实现密码加密存储
- 使用 HTTPS 传输
- 添加访问控制

### 2. UUID 唯一性

- UUID 是摄像头的唯一标识符
- 重复添加相同 UUID 会失败
- 建议使用有意义的命名（如 `cam_entrance_001`）

### 3. 事务一致性

- 添加/更新操作使用数据库事务
- 要么全部成功，要么全部回滚
- 保证数据一致性

### 4. 并发访问

- CameraStorage 使用单连接池
- 所有操作串行执行
- 适合小规模场景（< 100 设备）

---

## 🔗 相关文档

- [Camera 模块架构](../../camera/README.md)
- [SQLite 事务与连接池指南](../../sqlite/TRANSACTION_AND_POOL_GUIDE.md)
- [Web 模块文档](../README.md)

---

**文档版本：** v1.0  
**最后更新：** 2026-04-17  
**作者：** AI Assistant
