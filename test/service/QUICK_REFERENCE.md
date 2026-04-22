# Service 架构快速参考卡 🚀

## 📦 核心组件速查

### IService 接口
```cpp
initialize()    // 初始化（一次）
start()         // 启动
stop()          // 停止
GetName()       // 获取名称
isRunning()     // 运行状态？
isInitialized() // 初始化状态？
```

### ServiceContainer 单例
```cpp
// 获取实例
auto& container = ServiceContainer::getInstance();

// 注册服务
container.registerService<HttpServerService>(config.server);
container.registerService<ZLMService>(ctx, config.media);

// 批量操作
container.initializeAll();  // 初始化所有
container.startAll();       // 启动所有
container.stopAll();        // 停止所有（逆序）

// 获取服务
auto http_svc = container.getService<HttpServerService>();
auto zlm_svc = container.getService<ZLMService>();
```

## 🎯 三步添加新服务

### Step 1: 创建头文件
```cpp
// include/service/xxx_service.h
class XxxService : public IService {
    bool initialize() override;
    bool start() override;
    void stop() override;
    const char* GetName() const override { return "XxxService"; }
    bool isRunning() const override { return running_; }
    bool isInitialized() const override { return initialized_; }
};
```

### Step 2: 创建实现文件
```cpp
// src/service/xxx_service.cpp
bool XxxService::initialize() {
    if (initialized_) return true;
    // 初始化逻辑
    initialized_ = true;
    return true;
}

bool XxxService::start() {
    if (!initialized_) return false;
    if (running_) return true;
    // 启动逻辑
    running_ = true;
    return true;
}

void XxxService::stop() {
    if (!running_) return;
    // 停止逻辑
    running_ = false;
}
```

### Step 3: 注册使用
```cpp
container.registerService<XxxService>(/*参数*/);
```

## 🛠️ 现有服务清单

| 服务 | 头文件 | 实现 | 功能 |
|------|--------|------|------|
| HttpServerService | `http_server_service.h` | `http_server_service.cpp` | HTTP 服务器 |
| ZLMService | `zlm_service.h` | `zlm_service.cpp` | ZLMediaKit 流媒体 |

## 📝 完整使用流程

```cpp
#include "service/service_container.h"
#include "service/http_server_service.h"
#include "service/zlm_service.h"

int main() {
    // 1. 加载配置
    ConfigManager& cfg_mgr = ConfigManager::getInstance();
    cfg_mgr.load("tools/config.yaml");
    const auto& config = cfg_mgr.getConfig();
    
    // 2. 初始化日志
    LogManager& log_mgr = LogManager::getInstance();
    log_mgr.Init(config.log.dir, 1);
    
    // 3. 创建容器
    auto& container = ServiceContainer::getInstance();
    
    // 4. 注册服务
    container.registerService<HttpServerService>(config.server);
    
    boost::asio::io_context shared_ctx;
    container.registerService<ZLMService>(shared_ctx, config.media);
    
    // 5. 初始化
    container.initializeAll();
    
    // 6. 启动
    container.startAll();
    
    // 7. 运行
    auto http_svc = container.getService<HttpServerService>();
    http_svc->getIoContext()->run();
    
    // 8. 停止
    container.stopAll();
    
    // 9. 清理
    log_mgr.Shutdown();
}
```

## 🔍 常用宏

```cpp
LOG_MAIN_INFO_AT("消息：{}", 参数);
LOG_MAIN_ERROR_AT("错误：{}", 参数);
LOG_MAIN_WARN_AT("警告：{}", 参数);
```

## ⚡ 快速编译

### Visual Studio
1. 打开项目
2. 右键 → "删除缓存"
3. 生成 → "全部重新生成"
4. 运行 `test_service_arch.exe`

### CMake 命令行
```bash
cmake -DBUILD_SERVICE_TESTS=ON ..
cmake --build . --target test_service_arch
```

## 🎨 服务生命周期

```
创建 → initialize() → start() → running... → stop() → 销毁
         ↓              ↓                      ↓
      只执行一次    可多次调用              可多次调用
```

## 📊 服务状态转换

```
[未初始化] --initialize()--> [已初始化] --start()--> [运行中]
               ↑                ↑                      │
               │                │                      │
               └────────────────┴──────stop()─────────┘
```

## 💡 最佳实践

### ✅ 推荐
- 在 initialize() 中只做轻量级初始化
- 在 start() 中处理耗时操作
- 在 stop() 中确保资源释放
- 使用 try-catch 包装异常
- 记录详细的日志

### ❌ 避免
- 在 initialize() 中阻塞
- 忘记检查 initialized_ 状态
- 重复 start() 而不检查 running_
- 在 stop() 中抛出异常
- 忘记调用父类方法（如果有的话）

## 🧩 依赖管理

```cpp
// 服务 A 依赖服务 B
class AService : public IService {
public:
    AService(BService* b_svc) : b_svc_(b_svc) {}
    
    bool start() override {
        if (!b_svc_->isRunning()) {
            LOG_MAIN_ERROR_AT("Dependency B not running");
            return false;
        }
        // ...
    }
private:
    BService* b_svc_;
};

// 注册顺序
container.registerService<BService>();  // 先注册 B
container.registerService<AService>(b_svc);  // 后注册 A
```

## 🎯 测试模式

### 仅测试 HTTP
```cpp
const int test_mode = 0;  // 在 test_service_arch.cpp 中
```

### 测试 HTTP + ZLM
```cpp
const int test_mode = 1;  // 默认
```

## 📞 获取帮助

详细文档：
- `docs/SERVICE_ARCHITECTURE.md` - 完整使用指南
- `SERVICE_ARCHITECTURE_SUMMARY.md` - 实现总结

示例代码：
- `src/main_new.cpp` - 生产级示例
- `test/service/test_service_arch.cpp` - 测试示例

---

**快速记忆口诀：**
1. 继承 IService
2. 实现三方法（init/start/stop）
3. 注册到容器
4. 统一调用（initAll/startAll/stopAll）

就这么简单！✨
