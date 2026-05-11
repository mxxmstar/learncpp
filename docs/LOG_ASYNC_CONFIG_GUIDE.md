# 日志模块异步/同步配置指南

## 概述

日志模块现在支持通过配置项控制是否使用异步日志，方便调试和性能优化。

## 配置方式

### 1. YAML 配置文件方式

在 `tools/config.yaml` 中为每个日志器添加 `async` 字段：

```yaml
logs:
  mainlog:
    level: debug
    dir: ./logs
    rotation: daily
    max_file_size_mb: 100
    max_files: 5
    console: true
    json_format: false
    async: true  # true=异步(默认), false=同步(调试用)
    
  errorlog:
    level: error
    dir: ./logs
    rotation: daily
    console: true
    async: false  # 错误日志使用同步模式，确保立即输出
```

### 2. 代码中直接配置

```cpp
// 方式1：使用 LoggerConfig
LoggerConfig config("my_logger", spdlog::level::debug);
config.async = false;  // 设置为同步日志（调试模式）
LogManager::getInstance().RegisterLogger(config);

// 方式2：使用 LogConfig（从配置文件）
LogConfig cfg;
cfg.level = "debug";
cfg.dir = "./logs";
cfg.async = false;  // 设置为同步

auto logger_cfg = LogManager::ConvertToLoggerConfig(cfg, "my_logger");
LogManager::getInstance().ReloadFromConfig(logger_cfg);
```

## 异步 vs 同步对比

### 异步日志（async: true）- 默认模式

**优点：**
- 高性能：日志写入在后台线程进行，不阻塞主线程
- 适合生产环境：减少日志对业务逻辑的影响

**缺点：**
- 调试不便：日志可能延迟输出
- 程序崩溃时可能丢失部分日志
- 需要调用 `Flush()` 或等待才能看到完整日志

**适用场景：**
- 生产环境
- 高并发场景
- 性能敏感的代码路径

### 同步日志（async: false）- 调试模式

**优点：**
- 实时输出：日志立即写入，无需等待
- 调试友好：程序崩溃时不会丢失日志
- 顺序保证：日志严格按照执行顺序输出

**缺点：**
- 性能较低：每次日志都会阻塞当前线程
- 不适合高并发场景

**适用场景：**
- 开发和调试阶段
- 排查问题时
- 需要精确定位问题的场景

## 使用示例

### 示例1：调试时启用同步日志

```cpp
// 在程序启动时，将日志设为同步模式
LogManager& log_mgr = LogManager::getInstance();
log_mgr.Init("./logs", 1);

// 重新加载配置，使用同步模式
LoggerConfig config("main", spdlog::level::debug);
config.async = false;  // 关键：设置为同步
config.write_to_console = true;
log_mgr.ReloadFromConfig(config);

// 现在所有日志都会立即输出
LOG_MAIN_INFO_AT("这条日志会立即显示");
LOG_MAIN_ERROR_AT("错误也会立即显示");
```

### 示例2：混合使用异步和同步

```cpp
// 主日志使用异步（高性能）
LoggerConfig main_config("main", spdlog::level::info);
main_config.async = true;
LogManager::getInstance().RegisterLogger(main_config);

// 调试日志使用同步（实时输出）
LoggerConfig debug_config("debug", spdlog::level::debug);
debug_config.async = false;
LogManager::getInstance().RegisterLogger(debug_config);

// 使用时
LOG_MAIN_INFO("异步日志 - 高性能");
auto debug_logger = LogManager::getInstance().GetLogger("debug");
debug_logger->GetSpdLogger()->debug("同步日志 - 实时输出");
```

### 示例3：动态切换

```cpp
// 运行时可以重新配置
LoggerConfig config = ...;
config.async = false;  // 切换到同步模式进行调试
LogManager::getInstance().ReloadFromConfig(config);

// ... 调试代码 ...

config.async = true;  // 切换回异步模式
LogManager::getInstance().ReloadFromConfig(config);
```

## 注意事项

1. **默认行为**：如果不设置 `async` 字段，默认为 `true`（异步模式）

2. **Flush 行为**：
   - 异步模式：只在 `error` 级别及以上自动 flush
   - 同步模式：所有级别都立即 flush

3. **程序退出**：
   - 异步模式下，程序退出前务必调用 `LogManager::getInstance().FlushAll()`
   - 同步模式不需要特别处理

4. **性能影响**：
   - 同步模式会使日志调用变慢 10-100 倍（取决于存储介质）
   - 仅建议在调试时使用同步模式

5. **线程安全**：
   - 无论异步还是同步，日志器都是线程安全的
   - 异步模式使用 spdlog 的线程池
   - 同步模式直接使用 mutex 保护

## 测试

运行测试程序验证功能：

```bash
# 编译并运行测试
./bin/test_async_config
```

测试程序会演示：
- 异步日志的行为
- 同步日志的行为
- 从配置加载的方式

## 常见问题

### Q: 为什么我的日志没有立即输出？
A: 检查 `async` 是否为 `true`。如果是异步模式，需要调用 `Flush()` 或等待一段时间。

### Q: 调试时如何确保日志不丢失？
A: 将 `async` 设为 `false`，或者在关键位置调用 `logger->Flush()`。

### Q: 可以在运行时切换异步/同步吗？
A: 可以，调用 `ReloadFromConfig()` 并修改 `async` 字段即可。注意这会重建 logger。

### Q: 异步模式的性能优势有多大？
A: 在高并发场景下，异步模式可以减少 50%-90% 的日志开销，具体取决于日志频率和系统负载。

## 总结

- **开发/调试阶段**：设置 `async: false`，方便实时查看日志
- **生产环境**：设置 `async: true`，获得最佳性能
- **混合使用**：可以为不同的日志器设置不同的异步策略
- **灵活切换**：支持运行时动态修改配置
