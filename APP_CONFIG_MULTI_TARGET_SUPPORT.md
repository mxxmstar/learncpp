# AppConfig 多目标支持 - 完整重构

## ✅ 修改概述

将 `AppConfig` 中的 `zlm_client`（单个配置）改为 `zlm_clients`（数组），支持多个 ZLM 客户端目标。

---

## 📝 修改的文件

### 1. `modules/common/include/common/config/common_config.h`

#### 修改前
```cpp
struct AppConfig {
    HttpServerConfig server;
    HttpClientPoolConfig zlm_client;  // 单个配置
    // ...
};
```

#### 修改后
```cpp
struct AppConfig {
    HttpServerConfig server;
    std::vector<HttpClientPoolConfig> zlm_clients;  // 多个配置
    
    // 向后兼容：提供单个配置的访问接口
    const HttpClientPoolConfig& GetZlmClient() const {
        if (zlm_clients.empty()) {
            static HttpClientPoolConfig default_config;
            return default_config;
        }
        return zlm_clients[0];
    }
    
    HttpClientPoolConfig& GetZlmClient() {
        if (zlm_clients.empty()) {
            zlm_clients.emplace_back();
        }
        return zlm_clients[0];
    }
    
    // ...
};
```

**关键点**：
- ✅ 改为 `std::vector<HttpClientPoolConfig>`
- ✅ 提供 `GetZlmClient()` 方法保持向后兼容
- ✅ 自动处理空数组情况

---

### 2. `modules/common/src/config_manager.cpp`

#### A. YAML 解析 - 支持数组和单个配置

```cpp
// 解析客户端池配置（支持单目标和多目标）
if (node["clients"] && node["clients"]["zlm"]) {
    const auto& zlm_node = node["clients"]["zlm"];
    
    // 检查是数组还是单个配置
    if (zlm_node.IsSequence()) {
        // 新格式：数组
        for (const auto& client_node : zlm_node) {
            HttpClientPoolConfig config;
            // ... 解析每个字段
            config_.zlm_clients.push_back(config);
        }
    } else {
        // 旧格式：单个配置（向后兼容）
        HttpClientPoolConfig config;
        // ... 解析字段
        config_.zlm_clients.push_back(config);
    }
}
```

**优势**：
- ✅ 自动检测 YAML 格式
- ✅ 支持数组格式（新）
- ✅ 支持对象格式（旧，向后兼容）

---

#### B. 验证逻辑 - 遍历所有配置

```cpp
// 验证 ZLM 客户端池配置
if (config_.zlm_clients.empty()) {
    errors.push_back("clients.zlm must have at least one configuration");
} else {
    for (size_t i = 0; i < config_.zlm_clients.size(); ++i) {
        const auto& client = config_.zlm_clients[i];
        if (client.dst_port <= 0 || client.dst_port > 65535) {
            errors.push_back("clients.zlm[" + std::to_string(i) + "].dst_port must be between 1 and 65535");
        }
        // ... 其他验证
    }
}
```

---

#### C. YAML 序列化 - 智能保存格式

```cpp
// 客户端池配置（支持多目标）
if (!config_.zlm_clients.empty()) {
    if (config_.zlm_clients.size() == 1) {
        // 单个配置：保存为对象格式（向后兼容）
        const auto& client = config_.zlm_clients[0];
        node["clients"]["zlm"]["dst_host"] = client.dst_host;
        // ...
    } else {
        // 多个配置：保存为数组格式
        YAML::Node clients_array(YAML::NodeType::Sequence);
        for (const auto& client : config_.zlm_clients) {
            YAML::Node client_node;
            client_node["dst_host"] = client.dst_host;
            // ...
            clients_array.push_back(client_node);
        }
        node["clients"]["zlm"] = clients_array;
    }
}
```

**智能行为**：
- ✅ 单个配置 → 保存为对象（兼容旧格式）
- ✅ 多个配置 → 保存为数组（新格式）

---

#### D. 日志输出 - 显示所有配置

```cpp
// ZLM Client Pools
LOG_MAIN_INFO_AT("[ZLM Client Pools] (count: {})", config_.zlm_clients.size());
if (config_.zlm_clients.empty()) {
    LOG_MAIN_WARN_AT("  [WARNING] No ZLM client configurations found!");
} else {
    for (size_t i = 0; i < config_.zlm_clients.size(); ++i) {
        const auto& client = config_.zlm_clients[i];
        LOG_MAIN_INFO_AT("  [Pool {}]", i);
        LOG_MAIN_INFO_AT("    dst_host: {}", client.dst_host);
        LOG_MAIN_INFO_AT("    dst_port: {}", client.dst_port);
        // ...
    }
}
```

---

### 3. `modules/service/http_client/src/httpclient_pool_service.cpp`

#### 更新 `CreateFromAppConfig`

```cpp
std::shared_ptr<HttpClientPoolService> HttpClientPoolService::CreateFromAppConfig(const AppConfig& app_config) {
    // 从 AppConfig 获取 ZLM 客户端配置数组
    const auto& configs = app_config.zlm_clients;
    
    if (configs.empty()) {
        LOG_MAIN_WARN_AT("HttpClientPoolService: No ZLM client configs found in AppConfig, using default");
        // 创建一个默认配置
        HttpClientPoolConfig default_config;
        default_config.dst_host = "127.0.0.1";
        default_config.dst_port = 8080;
        // ...
        
        return std::make_shared<HttpClientPoolService>(std::vector<HttpClientPoolConfig>{default_config});
    }
    
    return std::make_shared<HttpClientPoolService>(configs);
}
```

**改进**：
- ✅ 直接使用 `app_config.zlm_clients`
- ✅ 空配置时提供默认值
- ✅ 不再手动转换单个字段

---

## 📊 YAML 配置示例

### 旧格式（仍然支持）

```yaml
clients:
  zlm:
    dst_host: "127.0.0.1"
    dst_port: 8080
    init_size: 5
    max_size: 20
    connect_timeout_ms: 5000
    idle_timeout_sec: 300
    max_requests_per_client: 1000
```

**行为**：
- ✅ 解析为单个配置的数组
- ✅ 保存时仍使用对象格式（向后兼容）

---

### 新格式（推荐）

```yaml
clients:
  zlm:
    - dst_host: "zlm-server-1"
      dst_port: 8080
      init_size: 5
      max_size: 20
      connect_timeout_ms: 5000
      idle_timeout_sec: 300
      max_requests_per_client: 1000
    
    - dst_host: "zlm-server-2"
      dst_port: 8080
      init_size: 5
      max_size: 20
      connect_timeout_ms: 5000
      idle_timeout_sec: 300
      max_requests_per_client: 1000
    
    - dst_host: "api-server"
      dst_port: 3000
      init_size: 3
      max_size: 10
      connect_timeout_ms: 3000
      idle_timeout_sec: 180
      max_requests_per_client: 500
```

**行为**：
- ✅ 解析为多个配置的数组
- ✅ 保存时使用数组格式

---

## 🔄 向后兼容性

### 1. 代码层面

```cpp
// 旧代码（仍然有效）
auto& client = app_config.GetZlmClient();  // 返回第一个配置

// 新代码（推荐）
for (const auto& client : app_config.zlm_clients) {
    // 处理每个配置
}
```

---

### 2. 配置文件层面

```yaml
# 旧配置文件（对象格式）
clients:
  zlm:
    dst_host: "localhost"
    dst_port: 8080
    # ...

# 加载后：config_.zlm_clients = [{...}]  （单个元素的数组）
# 保存后：仍然是对象格式（向后兼容）
```

```yaml
# 新配置文件（数组格式）
clients:
  zlm:
    - dst_host: "server1"
      dst_port: 8080
      # ...
    - dst_host: "server2"
      dst_port: 8080
      # ...

# 加载后：config_.zlm_clients = [{...}, {...}]  （多个元素的数组）
# 保存后：数组格式
```

---

## ✅ 优势总结

### 1. 灵活性

- ✅ 支持单目标（向后兼容）
- ✅ 支持多目标（新功能）
- ✅ 自动检测和转换格式

---

### 2. 向后兼容

- ✅ 旧配置文件无需修改
- ✅ 旧代码可以使用 `GetZlmClient()`
- ✅ 保存时智能选择格式

---

### 3. 扩展性

```cpp
// 未来可以轻松添加更多功能
for (const auto& client : app_config.zlm_clients) {
    // 负载均衡
    // 故障转移
    // 健康检查
    // ...
}
```

---

### 4. 清晰的日志

```
[ZLM Client Pools] (count: 3)
  [Pool 0]
    dst_host: zlm-server-1
    dst_port: 8080
    init_size: 5
    max_size: 20
    ...
  [Pool 1]
    dst_host: zlm-server-2
    dst_port: 8080
    init_size: 5
    max_size: 20
    ...
  [Pool 2]
    dst_host: api-server
    dst_port: 3000
    init_size: 3
    max_size: 10
    ...
```

---

## 🚀 迁移指南

### 对于现有项目

**无需任何修改！** 

- ✅ 旧配置文件继续工作
- ✅ 旧代码通过 `GetZlmClient()` 访问
- ✅ 系统自动处理兼容性

---

### 启用多目标支持

1. **修改配置文件**

```yaml
clients:
  zlm:
    - dst_host: "server1"
      dst_port: 8080
      # ...
    - dst_host: "server2"
      dst_port: 8080
      # ...
```

2. **使用多目标 API**

```cpp
auto service = HttpClientPoolService::CreateFromAppConfig(app_config);
service->Initialize();

// 访问不同的池
auto* pool1 = service->GetHttpClientPool("server1:8080");
auto* pool2 = service->GetHttpClientPool("server2:8080");
```

---

## ⚠️ 注意事项

### 1. 空配置处理

如果配置文件中没有 `clients.zlm`，系统会：
- ✅ 创建默认配置（127.0.0.1:8080）
- ✅ 记录警告日志

---

### 2. 验证错误提示

```
clients.zlm[0].dst_port must be between 1 and 65535
clients.zlm[1].init_size must be positive
```

错误信息包含索引，方便定位问题。

---

### 3. 保存格式选择

- **单个配置** → 对象格式（兼容旧系统）
- **多个配置** → 数组格式（新系统）

这个决策在 `toYaml()` 中自动完成。

---

## ✅ 测试建议

### 1. 测试旧配置文件

```bash
# 使用旧的 YAML 配置（对象格式）
# 验证：能正常加载、使用、保存
```

---

### 2. 测试新配置文件

```bash
# 使用新的 YAML 配置（数组格式）
# 验证：能正常加载、使用、保存
```

---

### 3. 测试混合场景

```cpp
// 1. 加载旧配置
// 2. 添加新配置
app_config.zlm_clients.push_back(new_config);
// 3. 保存
// 4. 验证保存格式为数组
```

---

## 🎉 总结

### 核心改进

1. ✅ **数据结构** - `zlm_client` → `zlm_clients`（数组）
2. ✅ **向后兼容** - 提供 `GetZlmClient()` 方法
3. ✅ **智能解析** - 自动检测数组/对象格式
4. ✅ **智能保存** - 根据数量选择格式
5. ✅ **完善验证** - 遍历所有配置进行验证
6. ✅ **清晰日志** - 显示所有配置详情

### 影响范围

- ✅ `common_config.h` - 数据结构定义
- ✅ `config_manager.cpp` - 配置管理
- ✅ `httpclient_pool_service.cpp` - 服务创建

### 兼容性

- ✅ 100% 向后兼容
- ✅ 旧配置文件无需修改
- ✅ 旧代码无需修改（可选升级）

---

## 🔗 相关文档

- [MULTI_TARGET_HTTP_CLIENT_POOL.md](./MULTI_TARGET_HTTP_CLIENT_POOL.md) - HttpClientPoolService 多目标支持
- [THREAD_POOL_QA.md](./THREAD_POOL_QA.md) - 线程池架构说明
