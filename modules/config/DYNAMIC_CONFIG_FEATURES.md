# ConfigManager 动态配置功能说明

## 🎯 功能概述

ConfigManager 现已支持完整的动态配置管理功能，包括：
1. ✅ **配置更新** - 原子化更新配置，无需重启
2. ✅ **版本管理** - 自动版本控制，支持回滚
3. ✅ **细粒度回调** - 字段级别的变更通知

---

## 📚 API 参考

### 1. 配置更新

#### `updateConfig()`

```cpp
/// @brief 更新整个配置（原子操作）
/// @param new_config 新配置
/// @return 成功返回 true
bool updateConfig(const AppConfig& new_config);
```

**特性**：
- ✅ 原子操作（线程安全）
- ✅ 自动验证配置
- ✅ 失败时自动回滚
- ✅ 触发所有回调

**使用示例**：

```cpp
auto& config_mgr = ConfigManager::getInstance();

// 获取当前配置
AppConfig new_config = config_mgr.getConfig();

// 修改需要的字段
new_config.server.port = 9090;
new_config.zlm.debug_terminal = false;

// 更新配置
if (config_mgr.updateConfig(new_config)) {
    std::cout << "Config updated! Version: " 
              << config_mgr.getConfigVersion() << std::endl;
} else {
    std::cerr << "Update failed (validation error?)" << std::endl;
}
```

---

### 2. 版本管理

#### `getConfigVersion()`

```cpp
/// @brief 获取配置版本号
/// @return 当前配置版本号
uint64_t getConfigVersion() const;
```

**使用示例**：

```cpp
uint64_t version = config_mgr.getConfigVersion();
std::cout << "Current config version: " << version << std::endl;
```

---

#### `rollbackToVersion()`

```cpp
/// @brief 回滚到指定版本
/// @param version 目标版本号
/// @return 成功返回 true
bool rollbackToVersion(uint64_t version);
```

**特性**：
- ✅ 最多保存 10 个历史版本
- ✅ 自动触发回调
- ✅ 线程安全

**使用示例**：

```cpp
// 记录当前版本
uint64_t safe_version = config_mgr.getConfigVersion();

// ... 进行一些配置修改 ...

// 如果出现问题，回滚到安全版本
if (something_wrong) {
    if (config_mgr.rollbackToVersion(safe_version)) {
        std::cout << "Rolled back to version " << safe_version << std::endl;
    }
}
```

---

### 3. 细粒度回调

#### `onFieldChange()`

```cpp
/// @brief 注册字段变更回调
/// @param field_path 字段路径，如 "server.port"
/// @param callback 回调函数
void onFieldChange(const std::string& field_path, FieldChangeCallback callback);
```

**支持的字段路径**：
- `server.host`
- `server.port`
- `zlm_client.dst_host`
- `zlm_client.dst_port`
- `zlm.zlm_host`
- `zlm.zlm_port`
- `websocket.host`
- `websocket.port`
- `camera_db.db_path`
- `user_db.db_path`

**使用示例**：

```cpp
// 监听端口变化
config_mgr.onFieldChange("server.port", 
    [](const std::string& field, 
       const std::any& old_value, 
       const std::any& new_value) {
        
        int old_port = std::any_cast<int>(old_value);
        int new_port = std::any_cast<int>(new_value);
        
        std::cout << "Port changed: " << old_port << " -> " << new_port << std::endl;
        
        // 通知 HTTP Server 更新端口
        auto http_svc = getHttpServerService();
        if (http_svc) {
            http_svc->updatePort(new_port);
        }
    });
```

---

#### `removeFieldChangeCallback()`

```cpp
/// @brief 移除字段变更回调
/// @param field_path 字段路径
void removeFieldChangeCallback(const std::string& field_path);
```

**使用示例**：

```cpp
// 移除不再需要的回调
config_mgr.removeFieldChangeCallback("server.port");
```

---

#### `setChangeCallback()` （全局回调）

```cpp
/// @brief 设置全局配置变更回调
void setChangeCallback(ConfigChangeCallback callback);
```

**使用示例**：

```cpp
config_mgr.setChangeCallback([](const AppConfig& new_config) {
    std::cout << "Configuration changed!" << std::endl;
    std::cout << "New server port: " << new_config.server.port << std::endl;
    
    // 通知所有相关服务
    notifyAllServices(new_config);
});
```

---

## 🔧 实际应用场景

### 场景 1：通过 API 动态更新配置

```cpp
// web/api/config_api.cpp
void handleUpdateConfig(HttpRequest& req, HttpResponse& resp) {
    auto& config_mgr = ConfigManager::getInstance();
    
    // 1. 解析请求体
    try {
        AppConfig new_config = req.getBodyAs<AppConfig>();
        
        // 2. 更新配置
        if (config_mgr.updateConfig(new_config)) {
            resp.setStatus(200);
            resp.setBody({
                "status": "success",
                "version": config_mgr.getConfigVersion()
            });
        } else {
            resp.setStatus(400);
            resp.setBody({"status": "error", "message": "Validation failed"});
        }
    } catch (...) {
        resp.setStatus(500);
        resp.setBody({"status": "error", "message": "Invalid JSON"});
    }
}
```

**API 调用示例**：

```bash
curl -X POST http://localhost:8080/api/config/update \
  -H "Content-Type: application/json" \
  -d '{
    "server": {
      "host": "127.0.0.1",
      "port": 9090
    },
    "zlm": {
      "zlm_port": 9999
    }
  }'
```

---

### 场景 2：HTTP Server 动态更新端口

```cpp
class HttpServerService : public IService {
public:
    bool initialize() override {
        // 注册端口变更回调
        ConfigManager::getInstance().onFieldChange("server.port",
            [this](const std::string&, const std::any&, const std::any& new_value) {
                int new_port = std::any_cast<int>(new_value);
                this->updatePort(new_port);
            });
        
        return true;
    }
    
    void updatePort(int new_port) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (running_) {
            // 停止旧服务器
            server_->stop();
            
            // 创建新服务器
            server_ = std::make_unique<AsioHttpServer>(*io_context_, worker_pool_, new_port);
            
            // 重新启动
            server_->start();
            
            LOG_MAIN_INFO_AT("HTTP Server port updated to {}", new_port);
        }
    }
};
```

---

### 场景 3：配置文件监控自动重载

```cpp
// main.cpp
int main() {
    auto& config_mgr = ConfigManager::getInstance();
    config_mgr.load("config.yaml");
    
    // 启动后台线程监控配置文件
    std::thread config_watcher([&]() {
        while (Application::getInstance().isRunning()) {
            config_mgr.checkAndReload();  // 检测文件变化并自动重载
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    });
    config_watcher.detach();
    
    return app.run();
}
```

---

### 场景 4：配置回滚机制

```cpp
class ConfigGuard {
public:
    ConfigGuard() {
        safe_version_ = ConfigManager::getInstance().getConfigVersion();
    }
    
    ~ConfigGuard() {
        // 如果程序异常退出，自动回滚
        if (!committed_) {
            std::cerr << "Config not committed, rolling back..." << std::endl;
            ConfigManager::getInstance().rollbackToVersion(safe_version_);
        }
    }
    
    void commit() {
        committed_ = true;
    }
    
private:
    uint64_t safe_version_;
    bool committed_ = false;
};

// 使用
void riskyOperation() {
    ConfigGuard guard;
    
    // 修改配置
    AppConfig new_config = getConfig();
    new_config.server.port = 9999;
    ConfigManager::getInstance().updateConfig(new_config);
    
    // 测试新配置
    if (testNewConfig()) {
        guard.commit();  // 提交，不回滚
    }
    // 如果抛出异常或忘记 commit，析构函数会自动回滚
}
```

---

## ⚠️ 注意事项

### 1. **线程安全**

所有方法都是线程安全的，内部使用 `std::mutex` 保护。

```cpp
// ✅ 安全：多线程同时更新
std::thread t1([&]() {
    AppConfig cfg = config_mgr.getConfig();
    cfg.server.port = 9090;
    config_mgr.updateConfig(cfg);
});

std::thread t2([&]() {
    AppConfig cfg = config_mgr.getConfig();
    cfg.zlm.zlm_port = 9999;
    config_mgr.updateConfig(cfg);
});
```

---

### 2. **版本历史限制**

最多保存 10 个历史版本，超出后最旧的版本会被删除。

```cpp
static constexpr size_t MAX_HISTORY_SIZE = 10;
```

如果需要更多历史版本，可以修改此常量。

---

### 3. **字段路径大小写敏感**

```cpp
// ✅ 正确
config_mgr.onFieldChange("server.port", callback);

// ❌ 错误（不会触发）
config_mgr.onFieldChange("Server.Port", callback);
```

---

### 4. **回调异常处理**

回调中的异常不会影响配置更新，但会打印错误信息。

```cpp
// 如果回调中抛出异常
config_mgr.onFieldChange("server.port", [](...) {
    throw std::runtime_error("Oops!");  // ← 会被捕获并打印
});

// 输出：[Config] Field callback exception: Oops!
```

---

### 5. **性能考虑**

- ✅ `updateConfig()` 是 O(1) 操作（除了回调）
- ✅ 版本历史占用内存：约 10 × sizeof(AppConfig) ≈ 几 KB
- ⚠️ 避免在回调中执行耗时操作

---

## 📊 完整示例

```cpp
#include "config/common_config.h"
#include <iostream>

int main() {
    auto& config_mgr = ConfigManager::getInstance();
    
    // 1. 加载初始配置
    config_mgr.load("config.yaml");
    std::cout << "Initial version: " << config_mgr.getConfigVersion() << std::endl;
    
    // 2. 注册字段回调
    config_mgr.onFieldChange("server.port", 
        [](const std::string&, const std::any& old_val, const std::any& new_val) {
            std::cout << "Port: " << std::any_cast<int>(old_val) 
                      << " -> " << std::any_cast<int>(new_val) << std::endl;
        });
    
    // 3. 注册全局回调
    config_mgr.setChangeCallback([](const AppConfig& cfg) {
        std::cout << "Config updated to version " 
                  << ConfigManager::getInstance().getConfigVersion() << std::endl;
    });
    
    // 4. 更新配置
    AppConfig new_config = config_mgr.getConfig();
    new_config.server.port = 9090;
    
    if (config_mgr.updateConfig(new_config)) {
        std::cout << "Update successful!" << std::endl;
    }
    
    // 5. 回滚（如果需要）
    // config_mgr.rollbackToVersion(0);
    
    return 0;
}
```

---

## 🎯 总结

### 优势

✅ **无需重启** - 运行时动态更新配置  
✅ **线程安全** - 支持并发更新  
✅ **自动验证** - 无效配置自动拒绝  
✅ **版本控制** - 支持回滚到历史版本  
✅ **细粒度通知** - 字段级别的变更回调  
✅ **易于扩展** - 可添加新的字段路径  

---

### 适用场景

- ✅ 开发环境快速调试配置
- ✅ 生产环境动态调整参数
- ✅ A/B 测试不同配置
- ✅ 灰度发布配置变更
- ✅ 紧急情况下快速回滚

---

## 🔗 相关文档

- [CONFIG_DUMP_FUNCTION.md](CONFIG_DUMP_FUNCTION.md) - dump 函数使用说明
- [CONFIG_STRUCT_SIMPLIFICATION.md](CONFIG_STRUCT_SIMPLIFICATION.md) - 结构体精简说明
- [CONFIG_NO_LOG_DEPENDENCY.md](CONFIG_NO_LOG_DEPENDENCY.md) - 移除 Log 依赖
