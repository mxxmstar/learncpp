# LogConfig 与 LoggerConfig 转换接口

## 🎯 问题背景

存在两个不同的配置结构体：

### LogConfig（配置文件使用）
```cpp
struct LogConfig {
    std::string level = "info";           // 字符串形式
    std::string dir = "./logs";
    size_t max_file_size_mb = 100;
    size_t max_files = 5;
    std::string rotation = "daily";       // 字符串形式
    bool console = true;
    bool json_format = false;
};
```

### LoggerConfig（日志模块使用）
```cpp
struct LoggerConfig {
    std::string name;
    spdlog::level::level_enum level;      // 枚举类型
    std::string log_dir;
    RotationPolicy policy;                // 枚举类型
    size_t max_file_size_mb;
    size_t max_files;
    bool write_to_main_log;
    bool write_to_console;
    bool is_json;
};
```

**问题：**
- ❌ 字段名称不一致
- ❌ 类型不同（字符串 vs 枚举）
- ❌ 需要转换才能使用

## ✅ 解决方案

在 `LogConfig` 中添加转换接口 `toLoggerConfig()`。

### 实现代码

```cpp
struct LogConfig {
    // ... 原有字段 ...
    
    /// @brief 转换为 LoggerConfig
    /// @param logger_name 日志器名称
    /// @return LoggerConfig 对象
    LoggerConfig toLoggerConfig(const std::string& logger_name = "main") const {
        LoggerConfig config(logger_name, parseLevel(level));
        config.log_dir = dir;
        config.policy = parseRotation(rotation);
        config.max_file_size_mb = max_file_size_mb;
        config.max_files = max_files;
        config.write_to_console = console;
        config.is_json = json_format;
        return config;
    }
    
private:
    /// @brief 解析日志级别字符串
    static spdlog::level::level_enum parseLevel(const std::string& level_str) {
        if (level_str == "trace") return spdlog::level::trace;
        if (level_str == "debug") return spdlog::level::debug;
        if (level_str == "info") return spdlog::level::info;
        if (level_str == "warn") return spdlog::level::warn;
        if (level_str == "error") return spdlog::level::err;
        if (level_str == "critical") return spdlog::level::critical;
        return spdlog::level::info;  // 默认
    }
    
    /// @brief 解析滚动策略字符串
    static RotationPolicy parseRotation(const std::string& rotation_str) {
        if (rotation_str == "daily") return RotationPolicy::DAILY;
        if (rotation_str == "weekly") return RotationPolicy::WEEKLY;
        if (rotation_str == "monthly") return RotationPolicy::MONTHLY;
        if (rotation_str == "hourly") return RotationPolicy::HOURLY;
        if (rotation_str == "size") return RotationPolicy::SIZE_BASED;
        return RotationPolicy::DAILY;  // 默认
    }
};
```

## 🔧 使用方法

### 方式 1：手动转换
```cpp
// 从配置文件加载
LogConfig log_config = config.log;

// 转换为 LoggerConfig
LoggerConfig logger_config = log_config.toLoggerConfig("main");

// 使用 LoggerConfig
log_mgr.ReloadFromConfig(logger_config);
```

### 方式 2：直接传递（推荐）
```cpp
// LogManager 提供了重载版本
log_mgr.ReloadFromConfig(config.log);  // ✅ 自动转换

// 内部调用链：
// ReloadFromConfig(LogConfig) 
//   → toLoggerConfig() 
//   → ReloadFromConfig(LoggerConfig)
```

## 📊 字段映射关系

| LogConfig 字段 | 类型 | → | LoggerConfig 字段 | 类型 |
|---------------|------|---|------------------|------|
| level | std::string | → | level | spdlog::level::level_enum |
| dir | std::string | → | log_dir | std::string |
| rotation | std::string | → | policy | RotationPolicy |
| max_file_size_mb | size_t | → | max_file_size_mb | size_t |
| max_files | size_t | → | max_files | size_t |
| console | bool | → | write_to_console | bool |
| json_format | bool | → | is_json | bool |
| - | - | - | name | std::string (参数指定) |
| - | - | - | write_to_main_log | bool (默认 true) |

## 💡 设计优势

### 1. 清晰的职责分离
```cpp
// LogConfig：负责从配置文件读取数据（字符串形式）
yaml:
  log:
    level: "info"
    rotation: "daily"

// LoggerConfig：负责日志模块内部使用（枚举类型）
LoggerConfig config("main", spdlog::level::info);
config.policy = RotationPolicy::DAILY;
```

### 2. 简化的使用方式
```cpp
// ❌ 旧方式：需要手动转换
LoggerConfig logger_config = config.log.toLoggerConfig("main");
log_mgr.ReloadFromConfig(logger_config);

// ✅ 新方式：直接传递
log_mgr.ReloadFromConfig(config.log);
```

### 3. 类型安全
```cpp
// 字符串自动转换为枚举
parseLevel("info") → spdlog::level::info
parseRotation("daily") → RotationPolicy::DAILY
```

### 4. 向后兼容
```cpp
// 旧的调用方式仍然有效
log_mgr.ReloadFromConfig(logger_config);  // ✅

// 新的调用方式更简洁
log_mgr.ReloadFromConfig(config.log);     // ✅
```

## ⚠️ 注意事项

### 1. 日志级别映射
```yaml
# 支持的日志级别
level: "trace"      # → spdlog::level::trace
level: "debug"      # → spdlog::level::debug
level: "info"       # → spdlog::level::info
level: "warn"       # → spdlog::level::warn
level: "error"      # → spdlog::level::err
level: "critical"   # → spdlog::level::critical

# 未知值会回退到 info
level: "unknown"    # → spdlog::level::info
```

### 2. 滚动策略映射
```yaml
# 支持的滚动策略
rotation: "daily"    # → RotationPolicy::DAILY
rotation: "weekly"   # → RotationPolicy::WEEKLY
rotation: "monthly"  # → RotationPolicy::MONTHLY
rotation: "hourly"   # → RotationPolicy::HOURLY
rotation: "size"     # → RotationPolicy::SIZE_BASED

# 未知值会回退到 daily
rotation: "unknown"  # → RotationPolicy::DAILY
```

### 3. 默认值
```cpp
// 未指定的字段使用默认值
LoggerConfig config = log_config.toLoggerConfig();
// config.name = "main"（默认）
// config.write_to_main_log = true（默认）
```

## 📝 完整示例

### YAML 配置文件
```yaml
log:
  level: "debug"
  dir: "./logs/"
  max_file_size_mb: 50
  max_files: 10
  rotation: "daily"
  console: true
  json_format: false
```

### C++ 代码
```cpp
#include "config/common_config.h"
#include "log/logmanager.h"

int main() {
    // 1. 加载配置
    ConfigManager& config_mgr = ConfigManager::getInstance();
    config_mgr.load("config.yaml");
    
    const auto& config = config_mgr.getConfig();
    
    // 2. 简单初始化日志
    LogManager& log_mgr = LogManager::getInstance();
    log_mgr.Init("./logs", 1);
    
    // 3. 重新加载日志配置（自动转换）
    log_mgr.ReloadFromConfig(config.log);  // ✅ 直接使用
    
    // 4. 使用日志
    LOG_MAIN_INFO_AT("Application started");
    LOG_MAIN_DEBUG_AT("Debug level: {}", config.log.level);
    
    return 0;
}
```

## ✅ 验证清单

- [x] `LogConfig::toLoggerConfig()` 方法已添加
- [x] `parseLevel()` 静态方法已添加
- [x] `parseRotation()` 静态方法已添加
- [x] `LogManager::ReloadFromConfig(LogConfig)` 重载方法已添加
- [x] 测试代码已更新
- [x] 向后兼容

## 🔍 相关文件

**修改的文件：**
- `include/config/common_config.h` - 添加转换接口
- `include/log/logmanager.h` - 添加重载方法
- `src/log/logmanager.cpp` - 实现重载方法

**关联文件：**
- `include/log/logger.h` - LoggerConfig 定义
- `tools/config.yaml` - 配置文件示例

---

**状态：** ✅ 已完成  
**影响范围：** 日志模块、配置模块  
**向后兼容：** 完全兼容
