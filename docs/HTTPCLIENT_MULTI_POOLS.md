# HttpClientPoolService 多连接池设计

## 🎯 问题背景

原来的单例设计存在问题：
- ❌ `HttpClientPool` 是单例，只能管理一个 host:port
- ❌ 如果需要向多个不同服务器发送请求，需要创建多个实例
- ❌ ZLMediaKit 只是其中一个使用场景

## ✅ 新设计：多连接池管理

### 核心改进

**设计思路：**
- ✅ `HttpClientPoolService` 不再是单例的包装器
- ✅ 它管理多个 `HttpClientPool` 实例（每个目标服务器一个）
- ✅ 采用延迟创建策略（第一次使用时才创建）
- ✅ 支持动态添加新的连接池

### 类结构

```cpp
class HttpClientPoolService : public IService {
private:
    boost::asio::io_context& ctx_;
    
    // 多个连接池，key 是 "host:port"
    std::unordered_map<std::string, std::unique_ptr<Net::HttpClientPool>> pools_;
    
    std::string default_pool_key_;  // 默认连接池的 key
};
```

## 🚀 使用方法

### 方式 1：自动创建默认池

```cpp
// 注册服务（不指定配置）
container.registerService<HttpClientPoolService>(shared_ctx);

// 在 ZLMService 中会自动创建默认池
auto pool = http_pool_svc->getOrCreatePool("127.0.0.1:8888", zlm_config);
```

### 方式 2：手动指定多个池

```cpp
auto& container = ServiceContainer::getInstance();
container.registerService<HttpClientPoolService>(shared_ctx);

auto* http_pool_svc = container.getService<HttpClientPoolService>();

// 创建 ZLM 连接池
Net::HttpClientPool::Config zlm_config;
zlm_config.host = "127.0.0.1";
zlm_config.port = 8888;
zlm_config.init_size = 5;
zlm_config.max_size = 20;
auto* zlm_pool = http_pool_svc->getOrCreatePool("zlm:8888", zlm_config);

// 创建另一个 API 服务器的连接池
Net::HttpClientPool::Config api_config;
api_config.host = "api.example.com";
api_config.port = 443;
api_config.init_size = 3;
api_config.max_size = 10;
auto* api_pool = http_pool_svc->getOrCreatePool("api:443", api_config);

// 获取已存在的池
auto* existing_pool = http_pool_svc->getPool("zlm:8888");

// 获取默认池（第一个创建的池）
auto* default_pool = http_pool_svc->getDefaultPool();
```

### 方式 3：在服务中使用

```cpp
class MyService : public IService {
public:
    bool initialize() override {
        auto* http_pool = ServiceContainer::getInstance()
            .getService<HttpClientPoolService>();
        
        if (!http_pool) {
            LOG_MAIN_ERROR_AT("HttpClientPoolService not found");
            return false;
        }
        
        // 创建或使用连接池
        Net::HttpClientPool::Config config;
        config.host = "example.com";
        config.port = 80;
        config.init_size = 5;
        
        auto* pool = http_pool->getOrCreatePool("example:80", config);
        
        // 使用连接池发起请求
        auto client = pool->Acquire();
        // ... 使用 client 发起 HTTP 请求
        pool->Release(client);
        
        return true;
    }
};
```

## 📊 连接池管理策略

### 延迟创建
```cpp
initialize() {
    // 初始化时不创建任何连接池
    // 等待第一次调用 getOrCreatePool 时才创建
    return true;
}
```

### 自动缓存
```cpp
getOrCreatePool(key, config) {
    // 1. 检查缓存
    if (pools_.count(key)) {
        return pools_[key].get();
    }
    
    // 2. 创建新池
    auto pool = std::make_unique<HttpClientPool>();
    pool->Init(ctx_, config);
    
    // 3. 存入缓存
    pools_[key] = std::move(pool);
    
    return pools_[key].get();
}
```

### 统一清理
```cpp
stop() {
    // 停止所有连接池
    for (auto& [key, pool] : pools_) {
        pool->Stop();
    }
    pools_.clear();
}
```

## 🎯 典型应用场景

### 场景 1：ZLMediaKit + 其他 API

```cpp
// 主程序
boost::asio::io_context shared_ctx;

auto& container = ServiceContainer::getInstance();

// 注册基础服务
container.registerService<HttpServerService>(config.server);
container.registerService<HttpClientPoolService>(shared_ctx);

// 注册业务服务
container.registerService<ZLMService>(shared_ctx, zlm_config);
container.registerService<CameraService>(shared_ctx, camera_config);
container.registerService<CloudApiService>(shared_ctx, cloud_config);

// 初始化
container.initializeAll();
```

```cpp
// ZLMService 内部
bool ZLMService::initialize() {
    auto* http_pool = ServiceContainer::getInstance()
        .getService<HttpClientPoolService>();
    
    // 为 ZLM 创建专用连接池
    Net::HttpClientPool::Config config;
    config.host = media_config_.zlm_host;
    config.port = media_config_.zlm_port;
    // ... 其他配置
    
    auto* zlm_pool = http_pool->getOrCreatePool(
        media_config_.zlm_host + ":" + std::to_string(media_config_.zlm_port),
        config
    );
    
    // 创建 ZLMApiClient（会使用这个连接池）
    zlm_manager_ = std::make_unique<ZLMManager>(ctx_, media_config_);
    
    return true;
}
```

### 场景 2：多云服务商 API

```cpp
class MultiCloudService : public IService {
private:
    Net::HttpClientPool* ali_pool_;
    Net::HttpClientPool* tencent_pool_;
    Net::HttpClientPool* aws_pool_;
    
public:
    bool initialize() override {
        auto* http_pool_svc = ServiceContainer::getInstance()
            .getService<HttpClientPoolService>();
        
        // 阿里云 API 连接池
        Net::HttpClientPool::Config ali_config;
        ali_config.host = "ecs.aliyuncs.com";
        ali_config.port = 443;
        ali_pool_ = http_pool_svc->getOrCreatePool("aliyun:443", ali_config);
        
        // 腾讯云 API 连接池
        Net::HttpClientPool::Config tencent_config;
        tencent_config.host = "cvm.tencentcloudapi.com";
        tencent_config.port = 443;
        tencent_pool_ = http_pool_svc->getOrCreatePool("tencent:443", tencent_config);
        
        // AWS API 连接池
        Net::HttpClientPool::Config aws_config;
        aws_config.host = "ec2.amazonaws.com";
        aws_config.port = 443;
        aws_pool_ = http_pool_svc->getOrCreatePool("aws:443", aws_config);
        
        return true;
    }
    
    void queryAliEcs() {
        auto client = ali_pool_->Acquire();
        // 发起请求到阿里云
        ali_pool_->Release(client);
    }
    
    void queryTencentCvm() {
        auto client = tencent_pool_->Acquire();
        // 发起请求到腾讯云
        tencent_pool_->Release(client);
    }
};
```

## 💡 设计优势

### 1. 灵活性
- ✅ 支持任意数量的连接池
- ✅ 每个池可以有独立的配置
- ✅ 动态添加新的连接池

### 2. 性能
- ✅ 延迟创建，避免不必要的资源占用
- ✅ 连接复用，提高性能
- ✅ 按需分配，节省内存

### 3. 可维护性
- ✅ 统一管理所有连接池
- ✅ 统一的 lifecycle 管理
- ✅ 清晰的日志和监控

### 4. 扩展性
- ✅ 易于添加新的目标服务器
- ✅ 支持运行时动态配置
- ✅ 可以实现连接池的热插拔

## ⚠️ 注意事项

### 1. Pool Key 命名
建议使用有意义的名称：
```cpp
// ✅ 推荐：清晰的命名
"zlm:8888"
"camera-api:8080"
"cloud:443"

// ❌ 不推荐：模糊的命名
"pool1"
"default"
```

### 2. 配置优化
不同的服务器可能需要不同的配置：
```cpp
// 本地服务器：连接数可以多一些
config.init_size = 10;
config.max_size = 50;

// 远程 API：连接数适当减少
config.init_size = 3;
config.max_size = 10;
```

### 3. 错误处理
确保检查连接池是否创建成功：
```cpp
auto* pool = http_pool_svc->getOrCreatePool(key, config);
if (!pool) {
    LOG_MAIN_ERROR_AT("Failed to create pool");
    return false;
}
```

## 📚 相关文件

- `include/service/httpclient_pool_service.h` - 头文件
- `src/service/httpclient_pool_service.cpp` - 实现
- `docs/HTTPCLIENT_POOL_SERVICE.md` - 旧版本文档（单池设计）

---

**状态：** ✅ 已完成  
**影响范围：** Service 层、网络模块  
**向后兼容：** 需要更新使用代码
