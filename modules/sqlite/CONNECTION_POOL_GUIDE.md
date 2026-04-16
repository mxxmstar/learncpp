# SQLite 连接池使用指南

## 📋 概述

`SQLiteConnectionPool` 是一个高性能、线程安全的 SQLite 数据库连接池实现，提供：

- ✅ **连接复用** - 减少连接创建/销毁开销
- ✅ **自动扩容** - 根据负载动态调整连接数
- ✅ **超时控制** - 防止无限等待
- ✅ **健康检查** - 自动检测并替换失效连接
- ✅ **RAII 管理** - 自动释放连接，防止泄漏
- ✅ **统计信息** - 实时监控连接池状态
- ✅ **线程安全** - 支持多线程并发访问

---

## 🚀 快速开始

### 1. 基本用法

```cpp
#include "sqlite/connection_pool.h"

int main() {
    // 配置连接池
    SQLiteConnectionPool::Config config;
    config.db_path = "my_database.db";
    config.min_connections = 5;   // 最小连接数
    config.max_connections = 20;  // 最大连接数
    
    // 创建连接池
    SQLiteConnectionPool pool(config);
    
    // 获取连接（RAII，自动释放）
    auto conn = pool.acquire();
    
    // 执行 SQL
    const char* sql = "CREATE TABLE IF NOT EXISTS users (id INTEGER PRIMARY KEY, name TEXT)";
    sqlite3_exec(conn.get(), sql, nullptr, nullptr, nullptr);
    
    // 连接在作用域结束时自动释放回连接池
    return 0;
}
```

---

### 2. 多线程并发访问

```cpp
#include "sqlite/connection_pool.h"
#include <thread>
#include <vector>

void concurrentExample() {
    SQLiteConnectionPool::Config config;
    config.db_path = "concurrent.db";
    config.min_connections = 10;
    config.max_connections = 50;
    
    SQLiteConnectionPool pool(config);
    
    // 创建表
    {
        auto conn = pool.acquire();
        sqlite3_exec(conn.get(), 
            "CREATE TABLE IF NOT EXISTS data (id INTEGER PRIMARY KEY, value TEXT)",
            nullptr, nullptr, nullptr);
    }
    
    // 多线程并发插入
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&pool, i]() {
            for (int j = 0; j < 100; ++j) {
                auto conn = pool.acquire();
                
                std::string sql = "INSERT INTO data (value) VALUES ('thread_" + 
                                 std::to_string(i) + "_op_" + std::to_string(j) + "')";
                
                sqlite3_exec(conn.get(), sql.c_str(), nullptr, nullptr, nullptr);
                // 连接自动释放
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }
    
    // 查看统计信息
    pool.logStats();
}
```

---

### 3. 超时处理

```cpp
void timeoutExample() {
    SQLiteConnectionPool::Config config;
    config.db_path = "timeout.db";
    config.max_connections = 1;  // 只允许1个连接
    config.connection_timeout_ms = 1000;  // 1秒超时
    
    SQLiteConnectionPool pool(config);
    
    // 获取唯一的连接并保持
    auto conn1 = pool.acquire();
    
    // 尝试获取第二个连接（会超时）
    try {
        auto conn2 = pool.acquireWithTimeout(1000);
    } catch (const std::runtime_error& e) {
        std::cout << "Timeout: " << e.what() << std::endl;
    }
    
    // 释放第一个连接
    conn1.~PooledConnection();  // 或让它离开作用域
}
```

---

### 4. 非阻塞获取

```cpp
void nonBlockingExample() {
    SQLiteConnectionPool pool(/* config */);
    
    // 尝试获取连接（不阻塞）
    SQLiteConnectionPool::PooledConnection conn;
    if (pool.tryAcquire(conn)) {
        std::cout << "Connection acquired" << std::endl;
        // 使用 conn...
    } else {
        std::cout << "No connection available" << std::endl;
    }
}
```

---

### 5. 健康检查

```cpp
void healthCheckExample() {
    SQLiteConnectionPool::Config config;
    config.enable_health_check = true;
    config.health_check_interval_seconds = 60;  // 每60秒检查一次
    
    SQLiteConnectionPool pool(config);
    
    // 手动触发健康检查
    pool.healthCheck();
    
    // 查看统计
    pool.logStats();
}
```

---

## 📊 配置选项

### Config 结构

```cpp
struct Config {
    std::string db_path = ":memory:";           // 数据库路径
    int min_connections = 5;                     // 最小连接数
    int max_connections = 20;                    // 最大连接数
    int idle_timeout_seconds = 300;              // 空闲连接超时（秒）
    int connection_timeout_ms = 5000;            // 获取连接超时（毫秒）
    bool enable_health_check = true;             // 启用健康检查
    int health_check_interval_seconds = 60;      // 健康检查间隔（秒）
};
```

### 推荐配置

#### 低负载应用
```cpp
Config config;
config.min_connections = 3;
config.max_connections = 10;
config.idle_timeout_seconds = 120;
```

#### 中等负载应用
```cpp
Config config;
config.min_connections = 10;
config.max_connections = 50;
config.idle_timeout_seconds = 300;
```

#### 高负载应用
```cpp
Config config;
config.min_connections = 20;
config.max_connections = 100;
config.idle_timeout_seconds = 600;
config.enable_health_check = true;
```

---

## 🔍 监控与调试

### 获取统计信息

```cpp
auto stats = pool.getStats();

std::cout << "Total Connections: " << stats.total_connections << std::endl;
std::cout << "Active Connections: " << stats.active_connections << std::endl;
std::cout << "Idle Connections: " << stats.idle_connections << std::endl;
std::cout << "Total Acquired: " << stats.total_acquired << std::endl;
std::cout << "Total Released: " << stats.total_released << std::endl;
std::cout << "Timeout Count: " << stats.timeout_count << std::endl;
```

### 打印到日志

```cpp
pool.logStats();
```

输出示例：
```
=== SQLiteConnectionPool Statistics ===
Total Connections: 15
Active Connections: 8
Idle Connections: 7
Total Acquired: 1234
Total Released: 1220
Total Created: 20
Total Destroyed: 5
Timeout Count: 2
=====================================
```

---

## ⚠️ 注意事项

### 1. RAII 自动释放

`PooledConnection` 使用 RAII 模式，在析构时自动释放连接：

```cpp
{
    auto conn = pool.acquire();
    // 使用 conn...
}  // conn 离开作用域，自动释放回连接池
```

**不要手动调用 `release()`**，除非你非常清楚自己在做什么。

---

### 2. 移动语义

`PooledConnection` 支持移动，但不支持拷贝：

```cpp
auto conn1 = pool.acquire();

// ✅ 允许移动
auto conn2 = std::move(conn1);

// ❌ 不允许拷贝
auto conn3 = conn1;  // 编译错误
```

---

### 3. 异常安全

如果获取连接超时，会抛出 `std::runtime_error`：

```cpp
try {
    auto conn = pool.acquireWithTimeout(1000);
    // 使用 conn...
} catch (const std::runtime_error& e) {
    std::cerr << "Failed to acquire connection: " << e.what() << std::endl;
}
```

---

### 4. 线程安全

连接池是线程安全的，但 **SQLite 连接本身不是线程安全的**：

```cpp
// ✅ 正确：每个线程使用自己的连接
std::thread t1([&pool]() {
    auto conn = pool.acquire();
    // 使用 conn...
});

std::thread t2([&pool]() {
    auto conn = pool.acquire();  // 不同的连接
    // 使用 conn...
});

// ❌ 错误：不要在线程间共享 PooledConnection
auto conn = pool.acquire();
std::thread t1([conn]() { /* 使用 conn */ });  // 危险！
```

---

### 5. 优雅关闭

在程序退出前，应该显式关闭连接池：

```cpp
SQLiteConnectionPool pool(config);

// ... 使用连接池 ...

// 程序退出前
pool.shutdown();
```

如果不显式调用，析构函数会自动关闭。

---

## 🎯 最佳实践

### 1. 使用连接池单例

对于整个应用，建议使用单个连接池实例：

```cpp
class DatabaseService {
public:
    static DatabaseService& getInstance() {
        static DatabaseService instance;
        return instance;
    }
    
    SQLiteConnectionPool& getPool() { return pool_; }
    
private:
    DatabaseService() {
        SQLiteConnectionPool::Config config;
        config.db_path = "app.db";
        config.min_connections = 10;
        config.max_connections = 50;
        
        pool_ = SQLiteConnectionPool(config);
    }
    
    SQLiteConnectionPool pool_;
};
```

---

### 2. 批量操作使用事务

```cpp
void batchInsert(SQLiteConnectionPool& pool, const std::vector<std::string>& values) {
    auto conn = pool.acquire();
    
    // 开始事务
    sqlite3_exec(conn.get(), "BEGIN TRANSACTION", nullptr, nullptr, nullptr);
    
    try {
        for (const auto& value : values) {
            std::string sql = "INSERT INTO data (value) VALUES ('" + value + "')";
            sqlite3_exec(conn.get(), sql.c_str(), nullptr, nullptr, nullptr);
        }
        
        // 提交事务
        sqlite3_exec(conn.get(), "COMMIT", nullptr, nullptr, nullptr);
    } catch (...) {
        // 回滚事务
        sqlite3_exec(conn.get(), "ROLLBACK", nullptr, nullptr, nullptr);
        throw;
    }
}
```

---

### 3. 定期监控

```cpp
void monitorLoop(SQLiteConnectionPool& pool) {
    while (true) {
        pool.logStats();
        
        // 检查是否需要扩容
        auto stats = pool.getStats();
        if (stats.active_connections > stats.total_connections * 0.8) {
            LOG_WARN("Connection pool nearly exhausted!");
        }
        
        std::this_thread::sleep_for(std::chrono::minutes(5));
    }
}
```

---

### 4. 错误重试

```cpp
auto executeWithRetry(SQLiteConnectionPool& pool, const std::string& sql, int max_retries = 3) {
    for (int i = 0; i < max_retries; ++i) {
        try {
            auto conn = pool.acquire();
            int rc = sqlite3_exec(conn.get(), sql.c_str(), nullptr, nullptr, nullptr);
            
            if (rc == SQLITE_OK) {
                return true;
            }
            
            LOG_WARN("SQL execution failed, retry {}/{}", i+1, max_retries);
            
        } catch (const std::exception& e) {
            LOG_ERROR("Exception: {}", e.what());
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100 * (i + 1)));
    }
    
    return false;
}
```

---

## 🧪 运行测试

### 编译测试

```bash
cd modules/sqlite
mkdir build && cd build
cmake .. -DBUILD_SQLITE_TESTS=ON
cmake --build .
```

### 运行测试

```bash
./test_connection_pool
```

测试包括：
1. ✅ 基本用法
2. ✅ 并发访问
3. ✅ 超时处理
4. ✅ 非阻塞获取
5. ✅ 健康检查
6. ✅ 移动语义

---

## 📚 API 参考

### SQLiteConnectionPool

| 方法 | 说明 |
|------|------|
| `acquire()` | 获取连接（阻塞，使用配置的超时） |
| `acquireWithTimeout(ms)` | 获取连接（指定超时） |
| `tryAcquire(conn)` | 尝试获取连接（非阻塞） |
| `release(db)` | 释放连接（通常由 RAII 自动调用） |
| `shutdown()` | 关闭连接池 |
| `getStats()` | 获取统计信息 |
| `logStats()` | 打印统计信息到日志 |
| `healthCheck()` | 执行健康检查 |
| `isShutdown()` | 检查是否已关闭 |

### PooledConnection

| 方法 | 说明 |
|------|------|
| `get()` | 获取原始 `sqlite3*` 指针 |
| `operator->()` | 箭头操作符 |
| `operator bool()` | 检查连接是否有效 |

---

## 🔗 相关文档

- [SQLite 官方文档](https://www.sqlite.org/docs.html)
- [模块 README](README.md)
- [重构说明](REFACTORING.md)

---

## 💡 常见问题

### Q: 连接池大小应该设置为多少？

A: 取决于你的应用场景：
- **读多写少**: 可以设置较大的连接池（20-50）
- **写多读少**: SQLite 有写锁，连接池不宜过大（5-10）
- **混合负载**: 根据实际测试调整（10-30）

建议从较小的值开始，通过监控统计信息逐步调整。

---

### Q: 为什么需要健康检查？

A: 长时间运行的应用中，连接可能因为各种原因失效：
- 网络中断
- 数据库重启
- 系统资源不足

健康检查可以自动检测并替换失效连接，提高系统的可靠性。

---

### Q: 连接池会导致内存泄漏吗？

A: 不会。`PooledConnection` 使用 RAII 模式，确保连接总是被正确释放。即使发生异常，析构函数也会被调用。

---

### Q: 可以在不同线程间共享 PooledConnection 吗？

A: **不可以**。SQLite 连接不是线程安全的。每个线程应该从连接池获取自己的连接。

---

## 🎉 总结

`SQLiteConnectionPool` 提供了一个高效、安全、易用的 SQLite 连接管理方案：

- ✅ **简单易用** - RAII 自动管理
- ✅ **高性能** - 连接复用，减少开销
- ✅ **可扩展** - 自动扩容，适应负载
- ✅ **可靠** - 健康检查，超时控制
- ✅ **可监控** - 丰富的统计信息

开始使用连接池，提升你的应用性能和可靠性！
