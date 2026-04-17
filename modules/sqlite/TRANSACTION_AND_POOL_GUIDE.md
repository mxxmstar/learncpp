# SQLite 连接池与事务兼容性指南

## 📋 目录

- [问题背景](#问题背景)
- [根本原因分析](#根本原因分析)
- [解决方案对比](#解决方案对比)
- [方案1：单连接池（当前采用）](#方案1单连接池当前采用)
- [方案2：Transaction 持有连接（推荐用于高并发）](#方案2transaction-持有连接推荐用于高并发)
- [方案3：WAL 模式 + 改进锁机制](#方案3wal-模式--改进锁机制)
- [最佳实践建议](#最佳实践建议)

---

## 问题背景

### 现象

在使用 SQLite 连接池（pool_size > 1）时，事务操作失败：

```
error: database is locked
error: cannot commit - no transaction is active
error: cannot rollback - no transaction is active
```

### 典型场景

```cpp
// CameraStorage::Add() 示例
bool CameraStorage::Add(const CameraInfo& camera) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    SQLite::Transaction txn(*db_);  // 开始事务
    
    // 插入多个表
    db_->Insert("cameras_base", values1);
    db_->Insert("cameras_connection", values2);
    db_->Insert("cameras_protocol", values3);
    // ...
    
    txn.Commit();  // ❌ 失败：cannot commit - no transaction is active
}
```

---

## 根本原因分析

### SQLite 连接池的工作机制

```cpp
class SQLite {
private:
    ConnectionPool pool;  // 多个数据库连接
    
public:
    Error Execute(const std::string& sql) {
        // 每次调用都从连接池获取一个连接
        auto pooled_conn = pool.acquire();  // ← 获取连接 A
        sqlite3* db = pooled_conn.get();
        
        // 在该连接上执行 SQL
        sqlite3_exec(db, sql.c_str(), ...);
        
        // 函数返回时，连接自动释放回池
        return error;
    }  // ← 连接 A 释放回池
};
```

### 事务失败的时序图

```
线程 1                          连接池              数据库
  |                               |                    |
  |-- BeginTransaction() -------->|                    |
  |                               |-- acquire(连接A) -->|
  |                               |                    |-- BEGIN
  |                               |<-- release(A) -----|
  |                               |                    |
  |                               |                    |
  |-- Insert(table1) ------------>|                    |
  |                               |-- acquire(连接B) -->|  ❌ 不同连接！
  |                               |                    |-- INSERT (不在事务中)
  |                               |<-- release(B) -----|
  |                               |                    |
  |                               |                    |
  |-- Insert(table2) ------------>|                    |
  |                               |-- acquire(连接C) -->|  ❌ 又换了连接！
  |                               |                    |-- INSERT (不在事务中)
  |                               |<-- release(C) -----|
  |                               |                    |
  |                               |                    |
  |-- Commit() ------------------>|                    |
  |                               |-- acquire(连接D) -->|  ❌ 还是没有事务！
  |                               |                    |-- COMMIT (失败)
  |                               |<-- error -----------|
  |<-- "cannot commit" ----------|                    |
```

### 核心问题

**SQLite 的事务是基于连接的**：
- `BEGIN TRANSACTION` 在连接 A 上开启事务
- `INSERT` 在连接 B 上执行（不在事务中）
- `COMMIT` 在连接 C 上执行（没有活跃事务）

**每次 `Execute()` 调用都会获取不同的连接**，导致事务无法跨连接工作。

---

## 解决方案对比

| 方案 | 复杂度 | 性能 | 并发能力 | 适用场景 |
|------|--------|------|----------|----------|
| **方案1：单连接池** | ⭐ 简单 | ⚠️ 中等 | 低 | 小规模应用、已有 mutex 保护 |
| **方案2：Transaction 持有连接** | ⭐⭐⭐ 复杂 | ✅ 高 | 高 | 大规模应用、高并发写入 |
| **方案3：WAL + 改进锁** | ⭐⭐ 中等 | ✅ 高 | 中高 | 读多写少场景 |

---

## 方案1：单连接池（当前采用）

### 实现方式

```cpp
// CameraStorage::Init()
bool CameraStorage::Init(const std::string& db_path) {
    // 使用单连接池，避免事务问题
    db_ = std::make_unique<SQLite>(db_path, 1);  // pool_size = 1
    // ...
}
```

### 工作原理

```
线程 1                          连接池              数据库
  |                               |                    |
  |-- BeginTransaction() -------->|                    |
  |                               |-- acquire(连接A) -->|
  |                               |                    |-- BEGIN
  |                               |<-- release(A) -----|
  |                               |                    |
  |-- Insert(table1) ------------>|                    |
  |                               |-- acquire(连接A) -->|  ✅ 同一个连接
  |                               |                    |-- INSERT (在事务中)
  |                               |<-- release(A) -----|
  |                               |                    |
  |-- Insert(table2) ------------>|                    |
  |                               |-- acquire(连接A) -->|  ✅ 还是同一个连接
  |                               |                    |-- INSERT (在事务中)
  |                               |<-- release(A) -----|
  |                               |                    |
  |-- Commit() ------------------>|                    |
  |                               |-- acquire(连接A) -->|  ✅ 同一个连接
  |                               |                    |-- COMMIT (成功)
  |                               |<-- success ---------|
  |<-- OK -----------------------|                    |
```

### 优点

✅ **实现简单** - 无需修改 SQLite 模块  
✅ **事务可靠** - 所有操作在同一连接上  
✅ **内存占用低** - 只有一个连接  
✅ **适合当前场景** - CameraStorage 已有 `mutex_` 保护，本身就是串行访问  

### 缺点

❌ **并发性能受限** - 所有请求串行执行  
❌ **不适合高并发** - 多用户同时写入会排队  

### 适用场景

- ✅ Camera 设备管理（设备数量 < 100）
- ✅ 配置存储（低频写入）
- ✅ 日志记录（已有异步缓冲）
- ✅ 任何已有外部同步机制的场景

### 代码示例

```cpp
// modules/camera/src/camera_storage.cpp
bool CameraStorage::Init(const std::string& db_path) {
    try {
        LOG_MAIN_INFO_AT("Initializing CameraStorage: db_path={}", db_path);
        
        // 初始化 SQLite（使用单连接，避免事务问题）
        db_ = std::make_unique<SQLite>(db_path, 1);  // 连接池大小 1
        
        if (!CreateTables()) {
            LOG_MAIN_ERROR_AT("Failed to create camera tables");
            return false;
        }
        
        LOG_MAIN_INFO_AT("CameraStorage initialized successfully");
        return true;
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("CameraStorage init exception: {}", e.what());
        return false;
    }
}
```

---

## 方案2：Transaction 持有连接（推荐用于高并发）

### 设计思路

让 `Transaction` 对象持有连接池中的一个连接，所有事务内的操作都复用该连接。

### 架构设计

```
┌─────────────────────────────────────────────┐
│           SQLite (Connection Pool)          │
│                                             │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  │
│  │ Conn #1  │  │ Conn #2  │  │ Conn #3  │  │
│  └──────────┘  └──────────┘  └──────────┘  │
│       ↑              ↑              ↑       │
│       │              │              │       │
│  ┌────┴──────┐       │              │       │
│  │Transaction│       │              │       │
│  │ (holds    │       │              │       │
│  │  Conn #1) │       │              │       │
│  └───────────┘       │              │       │
│                      │              │       │
│  Regular Operations  │              │       │
│  (acquire/release)   │              │       │
└─────────────────────────────────────────────┘
```

### 实现步骤

#### 步骤 1：修改 SQLite 类，暴露连接获取接口

```cpp
// modules/sqlite/include/sqlite/sqlite.h
class SQLite {
public:
    // 新增：获取一个池化连接（不自动释放）
    class PooledConnection {
    public:
        explicit PooledConnection(std::shared_ptr<sqlite3> conn) 
            : conn_(std::move(conn)) {}
        
        sqlite3* get() const { return conn_.get(); }
        
    private:
        std::shared_ptr<sqlite3> conn_;
    };
    
    PooledConnection AcquireConnection();  // 手动获取连接
    
    // 现有方法保持不变
    Error Execute(const std::string& sql);
    Error Insert(const std::string& table, const std::map<std::string, std::string>& values);
    // ...
};
```

#### 步骤 2：实现 AcquireConnection

```cpp
// modules/sqlite/src/sqlite.cpp
SQLite::PooledConnection SQLite::AcquireConnection() {
    if (!impl_ || !impl_->pool) {
        throw std::runtime_error("Database not initialized");
    }
    
    // 从连接池获取连接（引用计数管理生命周期）
    auto pooled_conn = impl_->pool->acquire();
    return PooledConnection(pooled_conn);
}
```

#### 步骤 3：修改 Transaction 类

```cpp
// modules/sqlite/include/sqlite/sqlite.h
class Transaction {
public:
    explicit Transaction(SQLite& db);
    ~Transaction();
    
    void Commit();
    void Rollback();
    bool IsActive() const { return active_; }
    
    // 新增：获取事务使用的连接
    sqlite3* GetConnection() const { return conn_ ? conn_->get() : nullptr; }
    
private:
    SQLite& db_;
    SQLite::PooledConnection conn_;  // 持有连接
    bool active_ = true;
};
```

#### 步骤 4：实现新的 Transaction

```cpp
// modules/sqlite/src/sqlite.cpp
SQLite::Transaction::Transaction(SQLite& db) 
    : db_(db), conn_(db.AcquireConnection()) {  // 获取并持有连接
    
    // 在持有的连接上开始事务
    sqlite3* db_handle = conn_.get();
    char* errmsg = nullptr;
    int rc = sqlite3_exec(db_handle, "BEGIN TRANSACTION", nullptr, nullptr, &errmsg);
    
    if (rc != SQLITE_OK) {
        active_ = false;
        std::string error_msg = errmsg ? errmsg : "Unknown error";
        sqlite3_free(errmsg);
        LOG_MAIN_ERROR_AT("Failed to begin transaction: {}", error_msg);
        throw std::runtime_error(error_msg);
    }
}

SQLite::Transaction::~Transaction() {
    if (active_) {
        Rollback();
    }
    // conn_ 自动释放回连接池
}

void SQLite::Transaction::Commit() {
    if (active_) {
        sqlite3* db_handle = conn_.get();
        char* errmsg = nullptr;
        int rc = sqlite3_exec(db_handle, "COMMIT", nullptr, nullptr, &errmsg);
        
        if (rc != SQLITE_OK) {
            std::string error_msg = errmsg ? errmsg : "Unknown error";
            sqlite3_free(errmsg);
            LOG_MAIN_ERROR_AT("Failed to commit transaction: {}", error_msg);
            throw std::runtime_error(error_msg);
        }
        
        active_ = false;
    }
}

void SQLite::Transaction::Rollback() {
    if (active_) {
        sqlite3* db_handle = conn_.get();
        char* errmsg = nullptr;
        int rc = sqlite3_exec(db_handle, "ROLLBACK", nullptr, nullptr, &errmsg);
        
        if (rc != SQLITE_OK) {
            std::string error_msg = errmsg ? errmsg : "Unknown error";
            sqlite3_free(errmsg);
            LOG_MAIN_WARN_AT("Failed to rollback transaction: {}", error_msg);
        }
        
        active_ = false;
    }
}
```

#### 步骤 5：添加基于连接的操作方法

```cpp
// modules/sqlite/include/sqlite/sqlite.h
class SQLite {
public:
    // 新增：在指定连接上执行操作
    Error ExecuteOn(sqlite3* db, const std::string& sql);
    Error InsertOn(sqlite3* db, const std::string& table, 
                   const std::map<std::string, std::string>& values);
    Error UpdateOn(sqlite3* db, const std::string& table,
                   const std::map<std::string, std::string>& values,
                   const std::string& where,
                   const std::vector<std::string>& params);
    Error QueryOn(sqlite3* db, const std::string& sql, RowParser parser);
    Error QueryWithParamsOn(sqlite3* db, const std::string& sql,
                            const std::vector<std::string>& params,
                            RowParser parser);
};
```

#### 步骤 6：使用示例

```cpp
// 新的使用方式
bool CameraStorage::Add(const CameraInfo& camera) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        SQLite::Transaction txn(*db_);
        sqlite3* conn = txn.GetConnection();  // 获取事务连接
        
        // 检查是否存在
        bool exists = false;
        auto error = db_->QueryWithParamsOn(conn, 
            "SELECT COUNT(*) FROM cameras_base WHERE uuid = ?",
            {camera.GetUuid()},
            [&exists](void* stmt) {
                exists = (sqlite3_column_int(static_cast<sqlite3_stmt*>(stmt), 0) > 0);
            });
        
        if (error || exists) {
            txn.Rollback();
            return false;
        }
        
        // 所有操作都在同一个连接上
        db_->InsertOn(conn, "cameras_base", values1);
        db_->InsertOn(conn, "cameras_connection", values2);
        db_->InsertOn(conn, "cameras_protocol", values3);
        db_->InsertOn(conn, "cameras_video_params", values4);
        db_->InsertOn(conn, "cameras_status", values5);
        
        txn.Commit();  // ✅ 成功
        return true;
    } catch (...) {
        return false;
    }
}
```

### 优点

✅ **高性能** - 支持多连接并发  
✅ **事务可靠** - 连接由 Transaction 管理  
✅ **灵活** - 可以选择性使用事务连接  
✅ **向后兼容** - 现有代码不受影响  

### 缺点

❌ **实现复杂** - 需要大量修改 SQLite 模块  
❌ **API 变化** - 需要添加新的 `*On()` 方法  
❌ **学习成本** - 开发者需要理解连接管理  

### 适用场景

- ✅ 大规模应用（设备数量 > 1000）
- ✅ 高并发写入场景
- ✅ 需要细粒度控制连接的场景
- ✅ 微服务架构中的共享数据库

---

## 方案3：WAL 模式 + 改进锁机制

### 设计思路

启用 SQLite 的 WAL（Write-Ahead Logging）模式，允许多个读和一个写并发执行，减少锁竞争。

### WAL 模式简介

```
传统模式 (DELETE):
  写操作 → 锁定整个数据库 → 其他读写阻塞 → 解锁

WAL 模式:
  写操作 → 写入 WAL 文件 → 读操作从主文件读取 → 并发执行
```

### 实现步骤

#### 步骤 1：启用 WAL 模式

```cpp
// modules/sqlite/src/sqlite.cpp
bool SQLite::Init(const std::string& db_path, int pool_size) {
    // ... 现有初始化代码 ...
    
    // 启用 WAL 模式
    for (int i = 0; i < pool_size; ++i) {
        sqlite3* db = connections_[i].get();
        
        // 设置 WAL 模式
        char* errmsg = nullptr;
        int rc = sqlite3_exec(db, "PRAGMA journal_mode=WAL", 
                              nullptr, nullptr, &errmsg);
        
        if (rc != SQLITE_OK) {
            LOG_MAIN_WARN_AT("Failed to enable WAL mode: {}", 
                           errmsg ? errmsg : "Unknown error");
            sqlite3_free(errmsg);
        }
        
        // 优化 WAL 性能
        sqlite3_exec(db, "PRAGMA wal_autocheckpoint=1000", 
                     nullptr, nullptr, nullptr);
        sqlite3_exec(db, "PRAGMA synchronous=NORMAL", 
                     nullptr, nullptr, nullptr);
    }
    
    return true;
}
```

#### 步骤 2：使用 IMMEDIATE 事务

```cpp
// 普通事务（延迟锁定）
BEGIN TRANSACTION;          // 不立即加锁

// IMMEDIATE 事务（立即加写锁）
BEGIN IMMEDIATE TRANSACTION; // 立即获取写锁，避免升级死锁
```

```cpp
// modules/sqlite/src/sqlite.cpp
SQLite::Error SQLite::BeginTransaction() {
    // 使用 IMMEDIATE 模式，立即获取写锁
    return Execute("BEGIN IMMEDIATE TRANSACTION");
}
```

#### 步骤 3：配置 busy_timeout

```cpp
bool SQLite::Init(const std::string& db_path, int pool_size) {
    // ... 
    
    for (int i = 0; i < pool_size; ++i) {
        sqlite3* db = connections_[i].get();
        
        // 设置忙等待超时（5秒）
        sqlite3_busy_timeout(db, 5000);
        
        // ... 其他配置 ...
    }
}
```

### 工作原理

```
时间线     线程1 (写)              线程2 (读)           线程3 (写)
  |                                 |                    |
  |-- BEGIN IMMEDIATE ------------>|                    |
  |-- 获取写锁 -------------------->|                    |
  |                                 |                    |
  |-- INSERT into table1 ---------->|                    |
  |                                 |-- SELECT ---------|→ 从主文件读（不阻塞）
  |                                 |-- 返回结果 --------|→ ✅ 并发读取
  |                                 |                    |
  |-- INSERT into table2 ---------->|                    |
  |                                 |                    |-- BEGIN IMMEDIATE
  |                                 |                    |-- 等待写锁... ⏳
  |                                 |                    |
  |-- COMMIT ---------------------->|                    |
  |-- 释放写锁 -------------------->|                    |
  |                                 |                    |-- 获取写锁 ✅
  |                                 |                    |-- INSERT ...
  |                                 |                    |-- COMMIT
```

### 优点

✅ **读并发好** - 读写可以同时进行  
✅ **实现相对简单** - 主要是配置变更  
✅ **性能提升明显** - 适合读多写少场景  

### 缺点

❌ **写仍然串行** - 多个写操作仍需排队  
❌ **磁盘空间增加** - WAL 文件额外占用空间  
❌ **不完全解决事务问题** - 仍需要确保事务在同一个连接上  

### 适用场景

- ✅ 读多写少的应用（90% 读，10% 写）
- ✅ 报表系统、数据查询平台
- ✅ 配合方案1或方案2使用效果更佳

---

## 最佳实践建议

### 决策树

```
你的应用场景是什么？
│
├─ 设备/用户数量 < 100？
│  ├─ 是 → 使用【方案1：单连接池】✅ 简单可靠
│  └─ 否 ↓
│
├─ 写操作频率高（> 100次/秒）？
│  ├─ 是 → 使用【方案2：Transaction 持有连接】✅ 高性能
│  └─ 否 ↓
│
├─ 读操作远多于写操作（> 90% 读）？
│  ├─ 是 → 使用【方案3：WAL 模式】+ 方案1 ✅ 读并发好
│  └─ 否 → 使用【方案1：单连接池】✅ 平衡选择
```

### 当前项目建议

#### Camera 模块（已采用方案1）

```cpp
// ✅ 当前实现
db_ = std::make_unique<SQLite>(db_path, 1);
```

**理由：**
- Camera 设备数量有限（< 100）
- 写入频率低（注册、状态更新）
- 已有 `mutex_` 保护，串行访问
- 实现简单，维护成本低

#### 未来扩展建议

如果系统规模扩大：

1. **短期（设备 < 500）**
   - 保持方案1
   - 优化查询索引
   - 添加缓存层

2. **中期（设备 500-2000）**
   - 升级到方案2
   - 实现读写分离
   - 添加连接监控

3. **长期（设备 > 2000）**
   - 考虑分库分表
   - 引入消息队列缓冲写入
   - 评估迁移到 PostgreSQL/MySQL

### 通用建议

#### 1. 事务使用原则

```cpp
// ✅ 正确：事务内完成所有操作
{
    SQLite::Transaction txn(*db_);
    db_->Insert(...);
    db_->Update(...);
    db_->Delete(...);
    txn.Commit();
}

// ❌ 错误：事务外执行操作
db_->Insert(...);  // 不在事务中
{
    SQLite::Transaction txn(*db_);
    db_->Update(...);
    txn.Commit();
}
```

#### 2. 异常安全

```cpp
// ✅ 使用 RAII，自动回滚
try {
    SQLite::Transaction txn(*db_);
    // ... 操作 ...
    txn.Commit();  // 成功后标记为非活跃
} catch (...) {
    // 析构函数自动回滚
    throw;
}

// ❌ 手动管理，容易遗漏
txn.Begin();
try {
    // ... 操作 ...
    txn.Commit();
} catch (...) {
    txn.Rollback();  // 可能忘记调用
    throw;
}
```

#### 3. 批量操作优化

```cpp
// ✅ 使用事务包裹批量操作
{
    SQLite::Transaction txn(*db_);
    for (const auto& item : items) {
        db_->Insert(...);
    }
    txn.Commit();  // 一次性提交，速度快 10-100 倍
}

// ❌ 每条记录单独提交
for (const auto& item : items) {
    db_->Insert(...);  // 每次都 fsync，极慢
}
```

#### 4. 监控和诊断

```cpp
// 添加事务监控
class MonitoredTransaction {
public:
    MonitoredTransaction(SQLite& db, const std::string& operation)
        : txn_(db), operation_(operation), start_time_(now()) {}
    
    ~MonitoredTransaction() {
        if (txn_.IsActive()) {
            LOG_MAIN_WARN_AT("Transaction auto-rolled back: {}, duration: {}ms",
                           operation_, elapsed_ms(start_time_));
        }
    }
    
    void Commit() {
        txn_.Commit();
        LOG_MAIN_DEBUG_AT("Transaction committed: {}, duration: {}ms",
                        operation_, elapsed_ms(start_time_));
    }
    
private:
    SQLite::Transaction txn_;
    std::string operation_;
    std::chrono::steady_clock::time_point start_time_;
};

// 使用
MonitoredTransaction txn(*db_, "AddCamera");
// ... 操作 ...
txn.Commit();
```

---

## 参考资料

- [SQLite WAL Mode](https://www.sqlite.org/wal.html)
- [SQLite Transaction Control](https://www.sqlite.org/lang_transaction.html)
- [SQLite Concurrency](https://www.sqlite.org/lockingv3.html)
- [Connection Pooling Best Practices](https://github.com/cpp-best-practices/connection_pooling)

---

**文档版本：** v1.0  
**最后更新：** 2026-04-17  
**作者：** AI Assistant  
**审核状态：** 待审核
