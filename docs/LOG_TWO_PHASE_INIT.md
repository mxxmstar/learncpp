# 日志模块两阶段初始化

## 🎯 问题背景

日志模块和配置模块的初始化顺序存在冲突：

```
问题：
1. 日志模块需要先初始化才能使用 LOG_* 宏
2. 但日志模块的配置（目录、级别、格式）在配置文件中
3. 配置文件需要在日志初始化之前加载
4. 形成循环依赖：日志 → 配置 → 日志
```

## ✅ 解决方案：两阶段初始化

### 第一阶段：简单初始化
```cpp
// 在 main() 或 test 函数开始
LogManager& log_mgr = LogManager::getInstance();
log_mgr.Init(config.log.dir, 1);  // 简单初始化，使用默认配置
```

**特点：**
- ✅ 快速初始化，不依赖配置文件
- ✅ 创建基本的 logger，可以输出日志
- ✅ 使用默认参数（目录、级别、格式）
- ⚠️ 日志级别为默认的 trace

### 第二阶段：重新加载配置
```cpp
// 加载配置文件后
ConfigManager& config_mgr = ConfigManager::getInstance();
config_mgr.load("../tools/config.yaml");

const auto& config = config_mgr.getConfig();

// 使用配置文件重新初始化日志
log_mgr.ReloadFromConfig(config.log);
```

**特点：**
- ✅ 使用配置文件中的实际参数
- ✅ 更新日志级别、格式等
- ✅ 保持 logger 实例不变，只更新配置
- ✅ 线程安全

## 📊 完整的初始化流程

```cpp
int main() {
    // ========== 第一阶段：简单初始化 ==========
    // 1. 先初始化日志（不依赖配置文件）
    LogManager& log_mgr = LogManager::getInstance();
    log_mgr.Init("./logs", 1);  // 使用默认目录
    
    // 2. 加载配置（此时已有日志，可以输出错误信息）
    ConfigManager& config_mgr = ConfigManager::getInstance();
    if (!config_mgr.load("../tools/config.yaml")) {
        LOG_MAIN_ERROR_AT("Failed to load config");  // ✅ 可以使用日志了
        return 1;
    }
    
    const auto& config = config_mgr.getConfig();
    
    // ========== 第二阶段：重新加载配置 ==========
    // 3. 使用配置文件中的参数重新初始化日志
    log_mgr.ReloadFromConfig(config.log);
    
    // 4. 现在可以使用完整的日志功能了
    LOG_MAIN_INFO_AT("Application starting...");
    LOG_MAIN_INFO_AT("Config loaded from: {}", config_mgr.getConfigPath());
    
    // ... 其他代码
}
```

## 🔧 实现细节

### LogManager::Init (第一阶段)
```cpp
void LogManager::Init(const std::string& base_dir, int async_threads) {
    if (initialized_) {
        return;  // 防止重复初始化
    }

    // 初始化线程池
    spdlog::init_thread_pool(8192, async_threads);

    // 初始化日志目录
    log_dir_ = base_dir;
    if (log_dir_.back() != '/') {
        log_dir_ += '/';
    }

    // 创建简单的日志器（默认配置）
    auto main_config = LoggerConfig("main", spdlog::level::trace);
    loggers_["main"] = std::make_shared<Logger>(main_config);
    auto error_config = LoggerConfig("error", spdlog::level::err);
    loggers_["error"] = std::make_shared<Logger>(error_config);
    
    initialized_ = true;
}
```

### LogManager::ReloadFromConfig (第二阶段)
```cpp
void LogManager::ReloadFromConfig(const LoggerConfig& config) {
    if (!initialized_) {
        // 如果还未初始化，先调用 Init
        Init();
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 重新配置 main logger
    auto it = loggers_.find("main");
    if (it != loggers_.end()) {
        // 更新现有 logger 的配置
        it->second->SetLevel(config.level);
        if (!config.format.empty()) {
            it->second->SetFormat(config.format);
        }
    }
    
    // 重新配置 error logger
    auto error_it = loggers_.find("error");
    if (error_it != loggers_.end()) {
        error_it->second->SetLevel(spdlog::level::err);
    }
}
```

## 💡 设计优势

### 1. 解决循环依赖
```
旧流程（循环依赖）：
加载配置 → 需要日志 → 初始化日志 → 需要配置 ❌

新流程（两阶段）：
简单初始化日志 → 加载配置 → 重新配置日志 ✅
```

### 2. 保持灵活性
```cpp
// 场景 1：不需要配置文件
log_mgr.Init("./logs");  // 直接使用默认配置

// 场景 2：有配置文件
config_mgr.load("config.yaml");
log_mgr.Init(config.log.dir);
log_mgr.ReloadFromConfig(config.log);  // 使用配置

// 场景 3：动态重新配置
log_mgr.ReloadFromConfig(new_config);  // 运行时更新配置
```

### 3. 向后兼容
```cpp
// 旧代码仍然有效
log_mgr.Init("./logs");  // ✅ 正常工作

// 新代码可以使用配置
log_mgr.ReloadFromConfig(config.log);  // ✅ 更灵活
```

## ⚠️ 注意事项

### 1. isInitialized() 检查
```cpp
if (!log_mgr.isInitialized()) {
    log_mgr.Init("./logs");
}
```

### 2. 配置文件的日志格式
```yaml
# tools/config.yaml
log:
  dir: "./logs/"
  level: "info"      # 日志级别
  format: "[%Y-%m-%d %H:%M:%S.%e] [%l] %v"  # 日志格式
```

### 3. 线程安全
```cpp
// ReloadFromConfig 内部已加锁，可以在任何线程调用
log_mgr.ReloadFromConfig(config);  // ✅ 线程安全
```

## 📝 使用示例

### 示例 1：基本使用
```cpp
int main() {
    // 第一阶段：简单初始化
    LogManager::getInstance().Init("./logs");
    
    // 加载配置
    ConfigManager::getInstance().load("config.yaml");
    
    // 第二阶段：重新加载配置
    LogManager::getInstance().ReloadFromConfig(
        ConfigManager::getInstance().getConfig().log
    );
    
    // 使用日志
    LOG_MAIN_INFO_AT("Application started");
    
    return 0;
}
```

### 示例 2：Service 架构
```cpp
class LogService : public IService {
public:
    bool initialize() override {
        // 第一阶段：已经在 main 中完成
        // 第二阶段：从配置重新加载
        LogManager::getInstance().ReloadFromConfig(config_.log);
        return true;
    }
};
```

## ✅ 验证清单

- [x] `Init()` 方法支持简单初始化
- [x] `ReloadFromConfig()` 方法支持重新配置
- [x] `isInitialized()` 方法检查初始化状态
- [x] 线程安全保护
- [x] 向后兼容
- [x] 测试代码已更新

## 🔍 相关文件

**修改的文件：**
- `include/log/logmanager.h` - 添加 ReloadFromConfig
- `src/log/logmanager.cpp` - 实现 ReloadFromConfig
- `test/service/test_service_arch.cpp` - 使用示例

**关联文件：**
- `include/config/common_config.h` - LoggerConfig 定义
- `include/log/logger.h` - Logger 类定义

---

**状态：** ✅ 已完成  
**影响范围：** 日志模块、配置模块  
**向后兼容：** 完全兼容
