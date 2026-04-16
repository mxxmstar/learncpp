# SQLite 连接池 - 快速参考

## 🚀 5分钟上手

### 1. 包含头文件

```cpp
#include "sqlite/connection_pool.h"
```

---

### 2. 创建连接池

```cpp
SQLiteConnectionPool::Config config;
config.db_path = "mydb.db";
config.min_connections = 5;
config.max_connections = 20;

SQLiteConnectionPool pool(config);
```

---

### 3. 获取连接并使用

```cpp
auto conn = pool.acquire();  // RAII，自动释放

const char* sql = "SELECT * FROM users";
sqlite3_stmt* stmt;
sqlite3_prepare_v2(conn.get(), sql, -1, &stmt, nullptr);

while (sqlite3_step(stmt) == SQLITE_ROW) {
    // 处理结果
}

sqlite3_finalize(stmt);
// conn 离开作用域时自动释放
```

---

### 4. 多线程使用

```cpp
std::vector<std::thread> threads;
for (int i = 0; i < 10; ++i) {
    threads.emplace_back([&pool]() {
        auto conn = pool.acquire();  // 每个线程独立获取
        // 使用 conn...
    });
}

for (auto& t : threads) {
    t.join();
}
```

---

## 📋 常用 API

### 获取连接

```cpp
// 阻塞获取（使用配置的超时）
auto conn = pool.acquire();

// 带超时获取
auto conn = pool.acquireWithTimeout(5000);  // 5秒

// 非阻塞尝试
SQLiteConnectionPool::PooledConnection conn;
if (pool.tryAcquire(conn)) {
    // 成功
}
```

---

### 执行 SQL

```cpp
auto conn = pool.acquire();

// 简单执行
sqlite3_exec(conn.get(), "CREATE TABLE test (id INTEGER)", nullptr, nullptr, nullptr);

// 预编译语句
sqlite3_stmt* stmt;
sqlite3_prepare_v2(conn.get(), "INSERT INTO test VALUES (?)", -1, &stmt, nullptr);
sqlite3_bind_int(stmt, 1, 42);
sqlite3_step(stmt);
sqlite3_finalize(stmt);
```

---

### 事务处理

```cpp
auto conn = pool.acquire();

sqlite3_exec(conn.get(), "BEGIN", nullptr, nullptr, nullptr);
try {
    // 执行多个 SQL
    sqlite3_exec(conn.get(), "INSERT ...", nullptr, nullptr, nullptr);
    sqlite3_exec(conn.get(), "UPDATE ...", nullptr, nullptr, nullptr);
    
    sqlite3_exec(conn.get(), "COMMIT", nullptr, nullptr, nullptr);
} catch (...) {
    sqlite3_exec(conn.get(), "ROLLBACK", nullptr, nullptr, nullptr);
    throw;
}
```

---

### 监控统计

```cpp
// 获取统计信息
auto stats = pool.getStats();
std::cout << "Active: " << stats.active_connections << std::endl;
std::cout << "Idle: " << stats.idle_connections << std::endl;

// 打印到日志
pool.logStats();

// 健康检查
pool.healthCheck();
```

---

## ⚙️ 配置参数

```cpp
struct Config {
    std::string db_path = ":memory:";           // 数据库路径
    int min_connections = 5;                     // 最小连接数
    int max_connections = 20;                    // 最大连接数
    int idle_timeout_seconds = 300;              // 空闲超时（秒）
    int connection_timeout_ms = 5000;            // 获取超时（毫秒）
    bool enable_health_check = true;             // 启用健康检查
    int health_check_interval_seconds = 60;      // 检查间隔（秒）
};
```

---

## 🎯 常见场景

### 场景 1: Web 服务器

```cpp
// 配置
Config config;
config.min_connections = 20;   // 保持较多连接
config.max_connections = 100;  // 支持高并发
config.connection_timeout_ms = 3000;

SQLiteConnectionPool pool(config);

// 每个请求
void handleRequest() {
    auto conn = pool.acquire();
    // 查询数据库
    // conn 自动释放
}
```

---

### 场景 2: 后台任务

```cpp
// 配置
Config config;
config.min_connections = 3;    // 少量连接
config.max_connections = 10;
config.idle_timeout_seconds = 60;  // 快速回收

SQLiteConnectionPool pool(config);

// 定期任务
void periodicTask() {
    auto conn = pool.acquire();
    // 执行任务
}
```

---

### 场景 3: 批量导入

```cpp
void batchImport(const std::vector<Data>& data) {
    auto conn = pool.acquire();
    
    sqlite3_exec(conn.get(), "BEGIN", nullptr, nullptr, nullptr);
    
    try {
        for (const auto& item : data) {
            // 插入数据
        }
        sqlite3_exec(conn.get(), "COMMIT", nullptr, nullptr, nullptr);
    } catch (...) {
        sqlite3_exec(conn.get(), "ROLLBACK", nullptr, nullptr, nullptr);
        throw;
    }
}
```

---

## ⚠️ 注意事项

### ✅ 正确做法

```cpp
// 1. 使用 RAII
{
    auto conn = pool.acquire();
    // 使用 conn
}  // 自动释放

// 2. 每个线程独立获取
std::thread t([&pool]() {
    auto conn = pool.acquire();
    // 使用 conn
});

// 3. 异常安全
try {
    auto conn = pool.acquireWithTimeout(5000);
    // 使用 conn
} catch (const std::runtime_error& e) {
    // 处理超时
}
```

---

### ❌ 错误做法

```cpp
// 1. 不要手动管理连接生命周期
auto conn = pool.acquire();
pool.release(conn.get());  // ❌ 让 RAII 处理

// 2. 不要在线程间共享连接
auto conn = pool.acquire();
std::thread t1([conn]() { /* ... */ });  // ❌ 危险
std::thread t2([conn]() { /* ... */ });  // ❌ 危险

// 3. 不要忘记检查返回值
sqlite3_exec(conn.get(), sql, ...);  // ❌ 应该检查返回值
```

---

## 🔍 调试技巧

### 1. 查看当前连接状态

```cpp
auto stats = pool.getStats();
std::cout << "Total: " << stats.total_connections << std::endl;
std::cout << "Active: " << stats.active_connections << std::endl;
std::cout << "Idle: " << stats.idle_connections << std::endl;
```

---

### 2. 检测连接泄漏

```cpp
// 程序运行一段时间后
pool.logStats();

// 如果 active_connections 持续增长，可能有泄漏
if (stats.active_connections > expected_max) {
    LOG_ERROR("Possible connection leak!");
}
```

---

### 3. 性能分析

```cpp
auto start = std::chrono::steady_clock::now();

for (int i = 0; i < 1000; ++i) {
    auto conn = pool.acquire();
    // 执行操作
}

auto end = std::chrono::steady_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

std::cout << "1000 operations in " << duration << "ms" << std::endl;
std::cout << "Throughput: " << (1000.0 / duration * 1000) << " ops/sec" << std::endl;
```

---

## 📊 性能调优

### 调整连接池大小

```cpp
// 监控活跃连接比例
auto stats = pool.getStats();
double utilization = (double)stats.active_connections / stats.total_connections;

if (utilization > 0.8) {
    // 连接池利用率过高，考虑增加 max_connections
    LOG_WARN("High utilization: {}%", utilization * 100);
}

if (utilization < 0.2 && stats.total_connections > config.min_connections) {
    // 连接池利用率过低，可以考虑减少
    LOG_INFO("Low utilization: {}%", utilization * 100);
}
```

---

### 调整超时时间

```cpp
// 根据实际响应时间调整
config.connection_timeout_ms = average_query_time * 3;

// 例如：平均查询 100ms，设置超时 300ms
config.connection_timeout_ms = 300;
```

---

## 🎓 最佳实践总结

1. **始终使用 RAII** - 让 `PooledConnection` 自动管理
2. **每个线程独立获取** - 不要共享连接
3. **使用事务** - 批量操作时提高性能
4. **定期监控** - 检查统计信息，及时发现问题
5. **优雅关闭** - 程序退出前调用 `shutdown()`
6. **合理配置** - 根据实际负载调整参数
7. **异常处理** - 捕获超时异常，实现重试逻辑

---

## 📚 更多信息

- [完整使用指南](CONNECTION_POOL_GUIDE.md)
- [实现总结](CONNECTION_POOL_SUMMARY.md)
- [测试代码](test/test_connection_pool.cpp)
