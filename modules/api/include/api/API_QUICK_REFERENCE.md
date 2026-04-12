# HTTP API 快速参考

## 📚 API 目录

### 流管理 API (`/stream/*`)

#### 添加拉流代理
```http
POST /stream/proxy/add
Content-Type: application/json

{
    "vhost": "__defaultVhost__",
    "app": "live",
    "stream": "cam1",
    "url": "rtsp://192.168.1.100/live",
    "rtp_type": 0  // 可选，默认 0(TCP)
}
```

#### 删除拉流代理
```http
DELETE /stream/proxy/delete
Content-Type: application/json

{
    "key": "__defaultVhost__/live/cam1"
}
```

#### 查询代理信息
```http
GET /stream/proxy/info?key=__defaultVhost__/live/cam1
```

#### 获取媒体流列表
```http
GET /stream/list
```

#### 获取媒体流信息
```http
GET /stream/info?app=live&stream=cam1
```

#### 关闭媒体流
```http
POST /stream/close
Content-Type: application/json

{
    "app": "live",
    "stream": "cam1"
}
```

---

### 系统管理 API (`/system/*`)

#### 获取系统状态
```http
GET /system/status
```

#### 获取系统配置
```http
GET /system/config
```

#### 重启系统
```http
POST /system/restart
```

---

### 测试 API

#### 健康检查
```http
GET /api/ping
```

**响应：**
```json
{
    "code": 200,
    "msg": "pong",
    "timestamp": "1743508800"
}
```

---

## 🔧 代码结构

### 文件位置

```
include/api/
├── stream_api_handler.h      # 流管理 API
├── system_api_handler.h      # 系统管理 API
└── api_router_registrar.h    # 路由注册器

src/api/
├── stream_api_handler.cpp
├── system_api_handler.cpp
└── api_router_registrar.cpp
```

### 如何添加新 API

**步骤 1：在 Handler 中添加处理函数**

```cpp
// include/api/stream_api_handler.h
class StreamApiHandler {
private:
    static void handleNewFeature(const boost::json::object& req, 
                                boost::json::object& rsp);
};

// src/api/stream_api_handler.cpp
void StreamApiHandler::Handle(const std::string& path, ...) {
    if (path == "/new-feature") {
        handleNewFeature(req, rsp);
    }
}

void StreamApiHandler::handleNewFeature(...) {
    // 实现逻辑
}
```

**步骤 2：无需修改 Registrar（自动支持）**

只要路径匹配，就会自动路由到对应的处理函数。

---

## 🎯 最佳实践

### 1. 参数验证

始终使用 `checkRequiredParams`：

```cpp
if (!checkRequiredParams(req, {"vhost", "app", "stream"}, rsp)) {
    return;  // rsp 已设置为 400
}
```

### 2. 错误处理

统一的错误响应格式：

```cpp
try {
    // 业务逻辑
}
catch (const std::exception& e) {
    rsp["code"] = 500;
    rsp["msg"] = std::string("Error: ") + e.what();
}
```

### 3. 日志记录

所有 Handler 都记录详细日志：

```cpp
LOG_MAIN_INFO_AT("Adding stream proxy: vhost={}, app={}, stream={}", 
                vhost, app, stream);
```

### 4. Service 访问

通过 `ServiceContainer` 获取服务：

```cpp
auto zlm_svc = ServiceContainer::getInstance().getService<ZLMService>();
if (!zlm_svc) {
    rsp["code"] = 503;
    rsp["msg"] = "Service not initialized";
    return;
}
```

---

## 📊 响应码说明

| Code | 含义 | 使用场景 |
|------|------|---------|
| 200 | 成功 | 操作成功完成 |
| 400 | 请求错误 | 缺少必需参数、参数格式错误 |
| 401 | 未授权 | 签名验证失败 |
| 404 | 未找到 | API 路径不存在 |
| 500 | 服务器错误 | 内部异常 |
| 503 | 服务不可用 | 依赖的服务未初始化 |

---

## 🚀 快速测试

### 使用 curl 测试

```bash
# 健康检查
curl http://127.0.0.1:8080/api/ping

# 添加拉流代理
curl -X POST http://127.0.0.1:8080/stream/proxy/add \
  -H "Content-Type: application/json" \
  -d '{
    "vhost": "__defaultVhost__",
    "app": "live",
    "stream": "cam1",
    "url": "rtsp://192.168.1.100/live"
  }'

# 获取系统状态
curl http://127.0.0.1:8080/system/status
```

### 使用 Postman 测试

1. 创建新请求
2. 设置方法和 URL
3. 设置 Body 为 JSON（如果是 POST/DELETE）
4. 发送请求

---

## 📖 相关文档

- [API 实现总结](./API_IMPLEMENTATION_SUMMARY.md) - 详细的架构设计和实现细节
- [HttpRouter 使用](../net/httprouter.h) - 路由分发机制
- [Service 容器](../service/service_container.h) - 服务管理
