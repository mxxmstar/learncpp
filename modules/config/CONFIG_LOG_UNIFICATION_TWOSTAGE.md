# Config 模块日志统一方案 - 两次初始化

## 🎯 方案概述

采用**两次初始化**策略，让 Config 模块统一使用 `LOG_MAIN_*_AT` 宏输出：

1. **第一次初始化**：程序启动时，LogManager 简单初始化（默认配置）
2. **第二次初始化**：Config 加载后，LogManager 根据配置重新初始化

---

## ✅ 实现步骤

### Step 1: main.cpp 中最早初始化 LogManager

```cpp
#include "log/logmanager.h"
#include "config/common_config.h"

int main() {
    // === 第一次初始化：LogManager 简单初始化 ===
    LogManager& log_mgr = LogManager::getInstance();
    log_mgr.Init("./logs", 1);  // 使用默认配置
    
    // 现在可以使用 LOG_MAIN_*_AT 宏了
    LOG_MAIN_INFO_AT("Application starting...");
    
    // === 加载配置（Config 模块可以使用日志宏）===
    ConfigManager& config_mgr = ConfigManager::getInstance();
    config_mgr.load("tools/config.yaml");
    
    // === 第二次初始化：LogManager 根据配置重新初始化 ===
    const auto& config = config_mgr.getConfig();
    if (config.logs.count("mainlog") > 0) {
        auto logger_cfg = log_utils::convertToLoggerConfig(config.logs.at("mainlog"));
        log_mgr.ReloadFromConfig(logger_cfg);
    }
    
    LOG_MAIN_INFO_AT("Logger reconfigured from config file");
    
    // ... 继续其他初始化
    return app.run();
}
```

---

## 📊 工作流程

```
程序启动
   │
   ├─→ LogManager::Init()              ← 第一次初始化（默认配置）
   │      └─→ 创建简单的 logger
   │
   ├─→ ConfigManager::load()           ← Config 加载
   │      └─→ 使用 LOG_MAIN_*_AT 输出  ← 统一使用 spdlog
   │
   ├─→ LogManager::ReloadFromConfig()  ← 第二次初始化（配置文件）
   │      └─→ 根据配置重新创建 logger
   │
   └─→ 应用程序运行
          └─→ 所有模块统一使用 LOG_MAIN_*_AT
```

---

## 💡 优势

### 1. **统一输出方式**
- ✅ Config 模块和其他模块都使用 `LOG_MAIN_*_AT`
- ✅ 不会出现 std::cout 和 spdlog 混用的问题
- ✅ 输出顺序一致，不会混乱

---

### 2. **灵活性高**
- ✅ LogManager 可以先用默认配置启动
- ✅ 配置加载后可以动态调整日志级别、路径等
- ✅ 支持热重载配置

---

### 3. **无循环依赖**
- ✅ LogManager 不依赖 ConfigManager
- ✅ ConfigManager 依赖 LogManager（单向依赖）
- ✅ 清晰的依赖关系

---

### 4. **早期日志支持**
- ✅ 即使在配置加载前也能记录日志
- ✅ 便于调试配置加载过程

---

## 🔧 Config 模块的修改

### 修改内容

1. **添加头文件**
```cpp
#include "log/logmanager.h"
```

2. **移除 std::iostream**
```cpp
// ❌ 删除
#include <iostream>
```

3. **替换所有输出为日志宏**

| 原代码 | 新代码 |
|--------|--------|
| `std::cout << msg << std::endl;` | `LOG_MAIN_INFO_AT("{}", msg);` |
| `std::cerr << msg << std::endl;` | `LOG_MAIN_ERROR_AT("{}", msg);` |
| `std::cout << "Port: " << port` | `LOG_MAIN_INFO_AT("Port: {}", port);` |

---

### 示例对比

#### 之前（std::cout/cerr）

```cpp
if (!std::filesystem::exists(config_path)) {
    std::cerr << "[Config] Config file '" << config_path 
              << "' not found" << std::endl;
    return false;
}
```

#### 之后（LOG_MAIN_*_AT）

```cpp
if (!std::filesystem::exists(config_path)) {
    LOG_MAIN_WARN_AT("[Config] Config file '{}' not found", config_path);
    return false;
}
```

---

## 📝 完整示例

```cpp
// main.cpp
#include "log/logmanager.h"
#include "config/common_config.h"
#include "application/application.h"

int main() {
    try {
        // === 阶段 1：LogManager 第一次初始化 ===
        LogManager& log_mgr = LogManager::getInstance();
        log_mgr.Init("./logs", 1);
        
        LOG_MAIN_INFO_AT("=== Application Starting ===");
        
        // === 阶段 2：加载配置（使用 LOG_MAIN_*_AT）===
        ConfigManager& config_mgr = ConfigManager::getInstance();
        
        if (!config_mgr.load("tools/config.yaml")) {
            LOG_MAIN_ERROR_AT("Failed to load config, using defaults");
        } else {
            LOG_MAIN_INFO_AT("Config loaded successfully");
        }
        
        // === 阶段 3：LogManager 第二次初始化（根据配置）===
        const auto& config = config_mgr.getConfig();
        if (config.logs.count("mainlog") > 0) {
            auto logger_cfg = log_utils::convertToLoggerConfig(
                config.logs.at("mainlog"), "main");
            log_mgr.ReloadFromConfig(logger_cfg);
            LOG_MAIN_INFO_AT("Logger reconfigured from config");
        }
        
        // === 阶段 4：打印配置（使用 LOG_MAIN_*_AT）===
        config_mgr.dump();
        
        // === 阶段 5：运行应用 ===
        auto& app = Application::getInstance();
        return app.run();
        
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("Fatal error: {}", e.what());
        return 1;
    }
}
```

**输出示例**：
```
2024-03-25 10:30:00.100 [info] === Application Starting ===
2024-03-25 10:30:00.101 [info] Config loaded successfully
2024-03-25 10:30:00.102 [info] Logger reconfigured from config
2024-03-25 10:30:00.103 [info] 
2024-03-25 10:30:00.103 [info] ========== AppConfig Dump ==========
2024-03-25 10:30:00.103 [info] [HTTP Server]
2024-03-25 10:30:00.103 [info]   host: 127.0.0.1
2024-03-25 10:30:00.103 [info]   port: 8080
...
```

---

## ⚠️ 注意事项

### 1. **必须在最早就初始化 LogManager**

```cpp
int main() {
    // ✅ 正确：最先初始化 LogManager
    LogManager::getInstance().Init();
    
    // 然后才能使用 LOG_MAIN_*_AT
    LOG_MAIN_INFO_AT("Starting...");
    
    // ❌ 错误：忘记初始化就使用日志宏
    // LOG_MAIN_INFO_AT("This will crash!");
}
```

---

### 2. **ReloadFromConfig 会重建 logger**

```cpp
// ReloadFromConfig 会：
// 1. 刷新旧的 logger
// 2. 创建新的 logger
// 3. 替换旧的 logger

log_mgr.ReloadFromConfig(new_config);
// ↑ 这会导致 logger 短暂不可用（微秒级）
```

---

### 3. **dump() 函数也使用日志宏**

```cpp
// dump() 不再使用 std::cout
config_mgr.dump();
// ↑ 输出通过 LOG_MAIN_INFO_AT，格式与其他日志一致
```

---

### 4. **CMakeLists.txt 需要链接 log_lib**

```cmake
target_link_libraries(config_lib
    PUBLIC
        yaml-cpp::yaml-cpp
        log_lib  # ← Config 模块现在依赖 log_lib
)
```

---

## 🎯 总结

### 核心思想

**两次初始化，统一输出**：
1. LogManager 先用默认配置初始化
2. Config 加载后，LogManager 根据配置重新初始化
3. 全程使用 `LOG_MAIN_*_AT`，无 std::cout 混用

---

### 关键步骤

1. ✅ main() 最开头调用 `LogManager::Init()`
2. ✅ Config 模块 include `"log/logmanager.h"`
3. ✅ 替换所有 `std::cout/cerr` 为 `LOG_MAIN_*_AT`
4. ✅ Config 加载后调用 `LogManager::ReloadFromConfig()`

---

### 效果

✅ **统一输出** - 所有模块都用 spdlog  
✅ **无混乱** - 输出顺序一致  
✅ **灵活配置** - 支持动态调整日志  
✅ **早期日志** - 配置加载前也能记录  

---

## 🔗 相关文档

- [DYNAMIC_CONFIG_FEATURES.md](DYNAMIC_CONFIG_FEATURES.md) - 动态配置功能
- [CONFIG_DUMP_FUNCTION.md](CONFIG_DUMP_FUNCTION.md) - dump 函数说明
- [CONFIG_STRUCT_SIMPLIFICATION.md](CONFIG_STRUCT_SIMPLIFICATION.md) - 结构体精简
