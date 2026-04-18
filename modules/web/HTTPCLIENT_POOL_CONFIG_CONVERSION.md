# HttpHttpClientPoolConfig 配置转换实现

## 🎯 问题

需要在 `httpclient_pool_service.cpp` 中将 `HttpHttpClientPoolConfig` 转换为 `Net::HttpClientPool::Config`。

---

## ✅ 解决方案

### 为什么不在结构体中添加转换方法？

**循环依赖问题**:
```
config_lib (HttpHttpClientPoolConfig)
    ↓ 需要返回 Net::HttpClientPool::Config
net_lib (HttpClientPool::Config)
```

如果 `HttpHttpClientPoolConfig` 包含返回 `Net::HttpClientPool::Config` 的方法，会导致：
- ❌ config 模块需要 include net 模块的头文件
- ❌ 形成循环依赖或紧耦合
- ❌ 违反模块分层原则

---

### 正确的做法：在使用处手动转换

**在 httpclient_pool_service.cpp 中**:

```cpp
// 手动转换配置
Net::HttpClientPool::Config pool_config;
pool_config.host = config_.dst_host;
pool_config.port = config_.dst_port;
pool_config.init_size = config_.init_size;
pool_config.max_size = config_.max_size;
pool_config.connect_timeout_ms = config_.connect_timeout_ms;
pool_config.idle_timeout_sec = config_.idle_timeout_sec;
pool_config.max_requests_per_client = config_.max_requests_per_client;

// 初始化连接池
pool_->Init(ctx_, pool_config);
```

---

## 📊 字段映射

| HttpHttpClientPoolConfig | Net::HttpClientPool::Config | 说明 |
|--------------------------|----------------------------|------|
| `dst_host` | `host` | 目标主机地址 |
| `dst_port` | `port` | 目标主机端口 |
| `init_size` | `init_size` | 初始连接数 |
| `max_size` | `max_size` | 最大连接数 |
| `connect_timeout_ms` | `connect_timeout_ms` | 连接超时（毫秒） |
| `idle_timeout_sec` | `idle_timeout_sec` | 空闲超时（秒） |
| `max_requests_per_client` | `max_requests_per_client` | 每客户端最大请求数 |

---

## 💡 优势

### 1. **避免循环依赖**
- ✅ config 模块不依赖 net 模块
- ✅ net 模块也不依赖 config 模块
- ✅ 清晰的单向依赖：web → config + net

---

### 2. **灵活性高**
- ✅ 可以在转换时添加额外的逻辑
- ✅ 可以根据条件设置不同的值
- ✅ 便于调试和日志记录

---

### 3. **职责清晰**
- ✅ config 模块只负责配置数据结构
- ✅ web 模块负责业务逻辑和类型转换
- ✅ 符合单一职责原则

---

## 🔧 如果需要复用转换逻辑

可以创建一个工具函数：

```cpp
// web/utils/config_converter.h
#pragma once
#include "config/common_config.h"
#include "net/httpclientpool.h"

namespace web_utils {
    inline Net::HttpClientPool::Config convertToPoolConfig(
        const HttpHttpClientPoolConfig& cfg) {
        
        Net::HttpClientPool::Config pool_config;
        pool_config.host = cfg.dst_host;
        pool_config.port = cfg.dst_port;
        pool_config.init_size = cfg.init_size;
        pool_config.max_size = cfg.max_size;
        pool_config.connect_timeout_ms = cfg.connect_timeout_ms;
        pool_config.idle_timeout_sec = cfg.idle_timeout_sec;
        pool_config.max_requests_per_client = cfg.max_requests_per_client;
        
        return pool_config;
    }
}
```

**使用**:
```cpp
auto pool_config = web_utils::convertToPoolConfig(config_);
pool_->Init(ctx_, pool_config);
```

---

## ⚠️ 注意事项

### 1. **字段名称差异**

配置结构体使用 `dst_host/dst_port`（更明确），而 HttpClientPool 使用 `host/port`。

**原因**: 
- ✅ `dst_` 前缀表明这是目标服务器
- ✅ 避免与本地服务器的 `host/port` 混淆

---

### 2. **保持同步**

如果 `Net::HttpClientPool::Config` 添加了新字段，记得在转换代码中也添加：

```cpp
// 如果 HttpClientPool::Config 新增字段
pool_config.new_field = config_.new_field;  // ← 不要忘记这里
```

---

### 3. **默认值**

确保转换时使用配置结构体的默认值，而不是硬编码：

```cpp
// ✅ 正确：使用配置结构体的值
pool_config.init_size = config_.init_size;  // 默认 5

// ❌ 错误：硬编码
pool_config.init_size = 5;  // 如果配置改了，这里不会生效
```

---

## 🎯 总结

### 核心原则

**在需要使用处进行转换，而不是在配置结构体中添加转换方法。**

---

### 优点

✅ **无循环依赖** - 模块之间解耦  
✅ **职责清晰** - 配置与业务分离  
✅ **灵活可扩展** - 易于添加转换逻辑  
✅ **易于维护** - 转换逻辑集中在一处  

---

### 适用场景

这种模式适用于：
- ✅ 不同模块之间的配置转换
- ✅ 第三方库的配置适配
- ✅ 避免循环依赖的场景

---

## 🔗 相关文档

- [CONFIG_STRUCT_SIMPLIFICATION.md](../config/CONFIG_STRUCT_SIMPLIFICATION.md) - Config 模块结构体精简
- [CONFIG_NO_LOG_DEPENDENCY.md](../config/CONFIG_NO_LOG_DEPENDENCY.md) - Config 模块移除 Log 依赖
