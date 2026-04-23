# HttpClientPool 去单例化重构

## 🎯 重构目标

将 `HttpClientPool` 从单例模式改为普通类，支持创建多个实例，以便向不同的服务器发送 HTTP 请求。

## ✅ 核心改动

### 1. HttpClientPool 头文件修改

**修改前：**
```cpp
class HttpClientPool {
public:
    static HttpClientPool& GetInstance();  // 单例访问点
    
private:
    HttpClientPool() = default;
    ~HttpClientPool();
    HttpClientPool(const HttpClientPool&) = delete;
    HttpClientPool& operator=(const HttpClientPool&) = delete;
};
```

**修改后：**
```cpp
class HttpClientPool {
public:
    /// @brief 默认构造函数
    HttpClientPool() = default;
    
    /// @brief 析构函数
    ~HttpClientPool();
    
    /// @brief 禁用拷贝构造和赋值
    HttpClientPool(const HttpClientPool&) = delete;
    HttpClientPool& operator=(const HttpClientPool&) = delete;
    
    /// @brief 初始化连接池
    void Init(boost::asio::io_context& io_context, const Config& config);
};
```

### 2. HttpClientPool 实现修改

**删除单例方法：**
```cpp
// 删除以下代码
HttpClientPool& HttpClientPool::GetInstance() {
    static HttpClientPool instance;
    return instance;
}
```

### 3. ZLMApiClient 依赖注入

**修改前：**
```cpp
class ZLMApiClient {
public:
    explicit ZLMApiClient(boost::asio::io_context& io_ctx, 
                         const ZLMAddressConfig& cfg);
private:
    boost::asio::io_context& io_context_;
    ZLMAddressConfig config_;
};
```

**修改后：**
```cpp
class ZLMApiClient {
public:
    explicit ZLMApiClient(boost::asio::io_context& io_ctx, 
                         Net::HttpClientPool* pool,  // ← 新增参数
                         const ZLMAddressConfig& cfg);
private:
    boost::asio::io_context& io_context_;
    Net::HttpClientPool* pool_;  // ← 新增成员
    ZLMAddressConfig config_;
};
```

### 4. ZLMRequestHelper 修改

**修改前：**
```cpp
void DoRequest(boost::asio::io_context& io_ctx,
              const ZLMAddressConfig& config,
              const std::string& api,
              const boost::json::object& params) {
    auto& pool = HttpClientPool::GetInstance();  // ← 使用单例
    // ...
}
```

**修改后：**
```cpp
void DoRequest(boost::asio::io_context& io_ctx,
              Net::HttpClientPool* pool,  // ← 新增参数
              const ZLMAddressConfig& config,
              const std::string& api,
              const boost::json::object& params) {
    if (!pool) {
        LOG_MAIN_ERROR_AT("HttpClientPool is null");
        return;
    }
    // 直接使用传入的 pool
}
```

### 5. ZLMManager 修改

**修改前：**
```cpp
explicit ZLMManager(boost::asio::io_context& ctx, 
                   const ZlmConfig& zlm_config);
```

**修改后：**
```cpp
explicit ZLMManager(boost::asio::io_context& ctx, 
                   Net::HttpClientPool* pool,  // ← 新增参数
                   const ZlmConfig& zlm_config);
```

### 6. HttpClientPoolService 修改

**修改前：**
```cpp
bool initialize() {
    pool_ = &Net::HttpClientPool::GetInstance();  // ← 获取单例
    pool_->Init(ctx_, config);
}

Net::HttpClientPool* pool_ = nullptr;  // 裸指针
```

**修改后：**
```cpp
bool initialize() {
    pool_ = std::make_unique<Net::HttpClientPool>();  // ← 创建新实例
    pool_->Init(ctx_, config);
}

std::unique_ptr<Net::HttpClientPool> pool_ = nullptr;  // 智能指针
```

## 📊 调用链变化

### 旧流程（单例模式）
```
main()
  ↓
ServiceContainer::initializeAll()
  ↓
HttpClientPoolService::initialize()
  → HttpClientPool::GetInstance()
  → pool->Init()
  
ZLMService::initialize()
  → new ZLMManager(ctx, config)
  → new ZLMApiClient(ctx, config)
  → ZLMRequestHelper::DoRequest(ctx, config, ...)
  → HttpClientPool::GetInstance()  ← 全局单例
```

### 新流程（依赖注入）
```
main()
  ↓
ServiceContainer::initializeAll()
  ↓
HttpClientPoolService::initialize()
  → pool_ = make_unique<HttpClientPool>()
  → pool_->Init(ctx_, config)
  
ZLMService::initialize()
  → http_pool_svc->GetHttpClientPool()  ← 从 Service 获取
  → new ZLMManager(ctx, pool, config)   ← 传递 pool
  → new ZLMApiClient(ctx, pool, config) ← 传递 pool
  → ZLMRequestHelper::DoRequest(ctx, pool, ...) ← 传递 pool
  → pool->AcquireGuard()  ← 使用传入的 pool
```

## 💡 设计优势

### 1. 支持多实例
```cpp
// 可以为不同的服务器创建不同的连接池
Net::HttpClientPool zlm_pool;
zlm_pool.Init(ctx, zlm_config);

Net::HttpClientPool camera_pool;
camera_pool.Init(ctx, camera_config);

Net::HttpClientPool cloud_pool;
cloud_pool.Init(ctx, cloud_config);
```

### 2. 更好的测试性
```cpp
// 单元测试中可以轻松创建 mock pool
Net::HttpClientPool mock_pool;
mock_pool.Init(ctx, test_config);

// 测试时不会影响到其他模块
```

### 3. 清晰的依赖关系
```cpp
// 显式传递依赖，代码更易理解
auto client = std::make_unique<ZLMApiClient>(ctx, pool, config);

// 而不是隐式依赖全局单例
auto client = std::make_unique<ZLMApiClient>(ctx, config);
// pool 从哪里来？不明确！
```

### 4. 生命周期管理更明确
```cpp
// Service 使用 unique_ptr 管理 pool
std::unique_ptr<Net::HttpClientPool> pool_;

// 自动释放，避免内存泄漏
```

## ⚠️ 注意事项

### 1. 所有使用 HttpClientPool 的地方都需要修改
需要检查并更新：
- ✅ `src/zlmediakit/zlm_httpclient.cpp`
- ✅ `src/zlmediakit/zlm_manager.cpp`
- ✅ `src/service/httpclient_pool_service.cpp`
- ✅ `src/service/zlm_service.cpp`
- ✅ 所有测试文件

### 2. 构造函数参数顺序
```cpp
// 新的参数顺序：io_context, pool, config
ZLMApiClient(io_ctx, pool, config)

// 不要搞错顺序！
```

### 3. 空指针检查
```cpp
// 在 DoRequest 中必须检查 pool 是否为空
if (!pool) {
    LOG_MAIN_ERROR_AT("HttpClientPool is null");
    return;
}
```

## 🔍 影响范围

### 修改的文件
- ✅ `include/net/httpclientpool.h` - 移除单例
- ✅ `src/net/httpclientpool.cpp` - 删除 GetInstance
- ✅ `include/zlmediakit/zlm_httpclient.h` - 添加 pool 参数
- ✅ `src/zlmediakit/zlm_httpclient.cpp` - 使用传入的 pool
- ✅ `include/zlmediakit/zlm_manager.h` - 添加 pool 参数
- ✅ `src/zlmediakit/zlm_manager.cpp` - 传递 pool
- ✅ `include/service/httpclient_pool_service.h` - 改用 unique_ptr
- ✅ `src/service/httpclient_pool_service.cpp` - 创建实例
- ✅ `src/service/zlm_service.cpp` - 获取并传递 pool

### 需要更新的测试文件
- `test/zlm/zlm.cpp`
- `test/net/httpclientpool.cpp`

## 📝 使用示例

### 场景 1：单个连接池（当前场景）
```cpp
// Service 中创建一个池
container.registerService<HttpClientPoolService>(ctx, server_config);

// ZLMService 中使用这个池
container.registerService<ZLMService>(ctx, media_config);
```

### 场景 2：多个连接池（未来扩展）
```cpp
// 为 ZLM 创建专用池
auto zlm_pool = std::make_unique<Net::HttpClientPool>();
zlm_pool->Init(ctx, zlm_config);

// 为相机 API 创建专用池
auto camera_pool = std::make_unique<Net::HttpClientPool>();
camera_pool->Init(ctx, camera_config);

// 为云服务创建专用池
auto cloud_pool = std::make_unique<Net::HttpClientPool>();
cloud_pool->Init(ctx, cloud_config);

// 在不同的 Service 中使用不同的池
zlm_manager = std::make_unique<ZLMManager>(ctx, zlm_pool.get(), zlm_config);
camera_manager = std::make_unique<CameraManager>(ctx, camera_pool.get(), camera_config);
cloud_manager = std::make_unique<CloudManager>(ctx, cloud_pool.get(), cloud_config);
```

## ✅ 验证清单

- [x] `HttpClientPool` 不再包含 `GetInstance()` 方法
- [x] 所有使用 `HttpClientPool` 的地方都改为接收 `pool` 参数
- [x] `HttpClientPoolService` 使用 `unique_ptr` 管理 `pool`
- [x] `ZLMApiClient` 构造函数接收 `pool` 参数
- [x] `ZLMRequestHelper::DoRequest` 接收 `pool` 参数
- [x] `ZLMManager` 构造函数接收 `pool` 参数
- [x] 所有调用链都已更新

---

**状态：** ✅ 已完成  
**影响范围：** 网络模块、ZLM 模块、Service 层  
**向后兼容：** ❌ 不兼容，需要更新所有调用代码
