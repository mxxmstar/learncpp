# 日志异步配置功能实现总结

## 修改内容

### 1. 配置文件结构体 (`common_config.h`)
- 在 `LogConfig` 结构体中添加了 `bool async = true;` 字段
- 默认值为 `true`，保持向后兼容

### 2. 日志器配置结构体 (`logger.h`)
- 在 `LoggerConfig` 结构体中添加了 `bool async = true;` 字段
- 添加注释说明用途

### 3. 日志器实现 (`logger.cpp`)
- 修改构造函数，根据 `config_.async` 决定创建同步还是异步 logger
- **异步模式**：使用 `spdlog::async_logger` + 线程池
- **同步模式**：使用普通的 `spdlog::logger`
- 同步模式下，所有级别的日志都会立即 flush（方便调试）
- 异步模式下，仅 error 及以上级别自动 flush（保持性能）

### 4. 配置解析 (`config_manager.cpp`)
- 在 YAML 解析中添加对 `async` 字段的读取
- 支持从配置文件控制异步/同步行为

### 5. 配置转换 (`logmanager.cpp`)
- 在 `ConvertToLoggerConfig` 函数中添加 `async` 字段的传递
- 确保配置能正确从 `LogConfig` 传递到 `LoggerConfig`

### 6. 配置文件示例 (`tools/config.yaml`)
- 为 mainlog 和 errorlog 添加 `async: true` 示例
- 添加注释说明用途

## 新增文件

### 1. 测试程序 (`test/log/test_async_config.cpp`)
演示三种使用方式：
- 异步日志测试
- 同步日志测试
- 从配置加载测试

### 2. 使用文档 (`docs/LOG_ASYNC_CONFIG_GUIDE.md`)
完整的使用指南，包括：
- 配置方式说明
- 异步 vs 同步对比
- 使用示例
- 注意事项
- 常见问题

## 使用方法

### 快速开始 - 调试时启用同步日志

**方法1：修改配置文件**
```yaml
logs:
  mainlog:
    level: debug
    dir: ./logs
    async: false  # 改为 false 启用同步模式
```

**方法2：代码中设置**
```cpp
LoggerConfig config("main", spdlog::level::debug);
config.async = false;  // 设置为同步
LogManager::getInstance().RegisterLogger(config);
```

### 生产环境 - 使用异步日志

```yaml
logs:
  mainlog:
    level: info
    async: true  # 或者不写，默认为 true
```

## 技术细节

### 异步日志器创建
```cpp
if (config_.async) {
    // 创建异步日志器
    spd_logger_ = std::make_shared<spdlog::async_logger>(
        config_.name,
        sinks.begin(),
        sinks.end(),
        spdlog::thread_pool(),
        spdlog::async_overflow_policy::block
    );
}
```

### 同步日志器创建
```cpp
else {
    // 创建同步日志器
    spd_logger_ = std::make_shared<spdlog::logger>(
        config_.name,
        sinks.begin(),
        sinks.end()
    );
}
```

### Flush 策略
```cpp
if (!config_.async) {
    spd_logger_->flush_on(spdlog::level::trace);  // 同步：所有级别立即flush
} else {
    spd_logger_->flush_on(spdlog::level::err);    // 异步：仅error及以上
}
```

## 兼容性

- ✅ 完全向后兼容：不设置 `async` 字段时默认为 `true`（异步）
- ✅ 不影响现有代码：所有现有代码继续正常工作
- ✅ 支持动态切换：可以在运行时通过 `ReloadFromConfig()` 切换

## 优势

1. **调试友好**：同步模式下日志立即输出，无需等待或手动 flush
2. **性能优化**：生产环境使用异步模式，减少日志对性能的影响
3. **灵活配置**：可以为不同的日志器设置不同的异步策略
4. **易于使用**：只需修改一个配置项即可切换

## 测试建议

1. 编译并运行测试程序验证功能
2. 在开发环境中设置 `async: false` 进行调试
3. 在生产环境中设置 `async: true` 获得最佳性能
4. 可以尝试混合使用：主日志异步，调试日志同步

## 注意事项

⚠️ **重要**：
- 同步模式会使日志调用变慢 10-100 倍
- 仅在调试时使用同步模式
- 异步模式下程序退出前务必调用 `FlushAll()`
- 同步模式下不需要特别处理退出

## 相关文件清单

### 修改的文件
- `modules/common/include/common/config/common_config.h`
- `modules/common/include/common/log/logger.h`
- `modules/common/src/logger.cpp`
- `modules/common/src/logmanager.cpp`
- `modules/common/src/config_manager.cpp`
- `tools/config.yaml`

### 新增的文件
- `test/log/test_async_config.cpp`
- `docs/LOG_ASYNC_CONFIG_GUIDE.md`
- `docs/LOG_ASYNC_CONFIG_IMPLEMENTATION.md` (本文件)
