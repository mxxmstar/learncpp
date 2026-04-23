# HttpClientPoolService 服务封装

## 🎯 背景

`ZLMApiClient` 依赖 `HttpClientPool` 单例，需要在使用前先初始化。为了统一管理，为 `HttpClientPool` 创建一个 Service 封装。

## ✅ 实现内容

### 1. 新增文件

#### 头文件
- ✅ `include/service/httpclient_pool_service.h`

#### 实现文件
- ✅ `src/service/httpclient_pool_service.cpp`

### 2. HttpClientPoolService 功能

```cpp
class HttpClientPoolService : public IService {
public:
    // 构造函数：接收 io_context 和 ServerConfig
    explicit HttpClientPoolService(boost::asio::io_context& ctx, const ServerConfig& config);
    
    // IService 接口实现
    bool initialize() override;  // 初始化连接池
    bool start() override;       // 启动服务
    void stop() override;        // 停止服务
    
    // 获取底层的 HttpClientPool
    Net::HttpClientPool* GetHttpClientPool();
};
```

### 3. 初始化逻辑

```cpp
bool HttpClientPoolService::initialize() {
    // 1. 获取 HttpClientPool 单例
    pool_ = &Net::HttpClientPool::GetInstance();
    
    // 2. 创建配置
    Net::HttpClientPool::Config pool_config;
    pool_config.host = config_.host;
    pool_config.port = config_.port;
    pool_config.init_size = 5;           // 初始连接数
    pool_config.max_size = 20;           // 最大连接数
    pool_config.connect_timeout_ms = 30000;
    pool_config.idle_timeout_sec = 300;
    pool_config.max_requests_per_client = 100;
    
    // 3. 初始化连接池
    pool_->Init(ctx_, pool_config);
}
```

### 4. ZLMService 依赖检查

修改 `ZLMService::initialize()` 添加依赖检查：

```cpp
bool ZLMService::initialize() {
    // ...
    
    // 确保 HttpClientPool 已经初始化（ZLMApiClient 依赖它）
    auto& http_pool_svc = ServiceContainer::getInstance().getService<HttpClientPoolService>();
    if (!http_pool_svc || !http_pool_svc->isInitialized()) {
        LOG_MAIN_ERROR_AT("HttpClientPoolService is not initialized");
        return false;
    }
    
    LOG_MAIN_INFO_AT("HttpClientPoolService is ready");
    
    // 创建 ZLMManager
    zlm_manager_ = std::unique_ptr<ZLMManager>(new ZLMManager(ctx_, config_));
}
```

## 📋 使用方式

### 在服务容器中注册

```cpp
auto& container = ServiceContainer::getInstance();

// 1. 注册 HTTP 服务器
container.registerService<HttpServerService>(config.server);

// 2. 注册 HttpClientPool（ZLM 依赖它）
container.registerService<HttpClientPoolService>(shared_ctx, config.server);

// 3. 注册 ZLMediaKit 服务
container.registerService<ZLMService>(shared_ctx, config.media);

// 4. 初始化所有服务（会按顺序初始化）
container.initializeAll();
```

## 🎯 服务依赖关系

```
┌─────────────────────┐
│  ServiceContainer   │
│                     │
│  初始化顺序：        │
│  1. HttpServer      │
│  2. HttpClientPool  │ ← ZLM 依赖它
│  3. ZLM             │
└─────────────────────┘
         ↓
┌─────────────────────┐
│  ZLMService         │
│  - 检查依赖          │
│  - 创建 ZLMManager  │
└─────────────────────┘
         ↓
┌─────────────────────┐
│  ZLMManager         │
│  - ZLMApiClient     │ ← 使用 HttpClientPool
│  - ZLMProcessManager│
│  - ZLMHookHandler   │
└─────────────────────┘
```

## 🔍 配置参数

HttpClientPool 的配置来自 `ServerConfig`：

```yaml
# tools/config.yaml
server:
  host: 127.0.0.1
  port: 8080
  # HttpClientPool 会使用这些配置连接到 HTTP 服务器
```

内部固定的配置：
- `init_size`: 5 - 初始连接数
- `max_size`: 20 - 最大连接数
- `connect_timeout_ms`: 30000 - 连接超时 30 秒
- `idle_timeout_sec`: 300 - 空闲超时 5 分钟
- `max_requests_per_client`: 100 - 每个连接最大请求数

## 💡 设计优势

### 1. 统一管理
- ✅ 所有服务都通过 ServiceContainer 管理
- ✅ 统一的生命周期接口
- ✅ 自动处理依赖关系

### 2. 延迟初始化
- ✅ 不需要在 main.cpp 中手动初始化 HttpClientPool
- ✅ 由 ServiceContainer 自动调用 initialize()

### 3. 依赖检查
- ✅ ZLMService 会检查 HttpClientPool 是否已初始化
- ✅ 避免运行时错误

### 4. 资源清理
- ✅ Stop() 时自动调用 HttpClientPool::Stop()
- ✅ RAII 模式确保资源释放

## ⚠️ 注意事项

### 1. 注册顺序
虽然 ServiceContainer 会按注册顺序初始化，但建议显式地先注册被依赖的服务：

```cpp
// ✅ 推荐：先注册 HttpClientPool
container.registerService<HttpClientPoolService>(ctx, config);
container.registerService<ZLMService>(ctx, config);

// ❌ 不推荐：顺序可能引起混淆
container.registerService<ZLMService>(ctx, config);
container.registerService<HttpClientPoolService>(ctx, config);
```

### 2. 共享 io_context
HttpClientPoolService 和 ZLMService 应该共享同一个 io_context：

```cpp
boost::asio::io_context shared_ctx;

container.registerService<HttpClientPoolService>(shared_ctx, config.server);
container.registerService<ZLMService>(shared_ctx, config.media);
```

### 3. 单例访问
`HttpClientPool` 是单例模式，Service 只是包装了初始化和清理逻辑。

## 📊 完整示例

参考 `test/service/test_service_arch.cpp` 中的 `testWithZLM()` 函数。

---

**状态：** ✅ 已完成  
**影响范围：** Service 层、ZLM 模块  
**向后兼容：** 需要更新调用代码（已更新测试程序）
