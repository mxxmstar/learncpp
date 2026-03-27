# SQLite 多实例架构指南

## 概述

SQLite 封装类采用**多实例架构**，可以同时操作多个不同的数据库文件。

## 核心特性

### ✅ 主要特点

1. **直接构造实例** - 可以创建多个独立的 SQLite 对象
2. **移动语义支持** - 支持所有权的转移
3. **自动资源管理** - 析构函数自动调用 Shutdown()
4. **RAII 风格** - 简洁、安全的资源管理

### 📝 变更内容

#### 头文件变化 (sqlite.h)

```cpp
class SQLite {
public:
    // 允许直接构造
    explicit SQLite(const std::string& db_path = ":memory:", int pool_size = 5);
    ~SQLite();
    
    // 禁止拷贝
    SQLite(const SQLite&) = delete;
    SQLite& operator=(const SQLite&) = delete;
    
    // 允许移动
    SQLite(SQLite&& other) noexcept;
    SQLite& operator=(SQLite&& other) noexcept;
    
    // ... 其他方法保持不变 ...
};
```

#### 实现文件变化 (sqlite.cpp)

添加了以下方法的实现：
- `SQLite::SQLite()` - 构造函数
- `SQLite::~SQLite()` - 析构函数
- `SQLite::SQLite(SQLite&&)` - 移动构造函数
- `SQLite::operator=(SQLite&&)` - 移动赋值运算符

## 使用示例

### 1. 多数据库并行操作

```cpp
// 创建多个独立的数据库实例
SQLite users_db("users.db", 5);      // 用户数据库
SQLite orders_db("orders.db", 5);    // 订单数据库
SQLite logs_db("logs.db", 2);        // 日志数据库

// 在各数据库中独立操作
users_db.CreateTable("users", {...});
orders_db.CreateTable("orders", {...});
logs_db.CreateTable("logs", {...});

// 跨数据库查询
int user_count, order_count;
users_db.Count("users", "", {}, user_count);
orders_db.Count("orders", "", {}, order_count);
```

### 2. 主从数据库架构

```cpp
// 主库 - 负责写操作
SQLite primary_db("primary.db", 10);

// 从库 - 负责读操作
SQLite replica_db("replica.db", 5);

// 写入主库
primary_db.Insert("users", {...});

// 从从库读取
replica_db.Query("SELECT * FROM users", callback);
```

### 3. 微服务架构

```cpp
// 使用智能指针管理多个服务数据库
std::map<std::string, std::unique_ptr<SQLite>> databases;

databases["user"] = std::make_unique<SQLite>("user_service.db");
databases["order"] = std::make_unique<SQLite>("order_service.db");
databases["product"] = std::make_unique<SQLite>("product_service.db");

// 使用各个服务的数据库
databases["user"]->Insert("users", {...});
databases["order"]->Query("SELECT * FROM orders", callback);
```

### 4. 依赖注入（单元测试）

```cpp
// 业务类
class UserService {
    SQLite& db_;
public:
    explicit UserService(SQLite& db) : db_(db) {}
    
    void createUser(...) {
        db_.Insert("users", ...);
    }
};

// 生产环境
SQLite prod_db("production.db");
UserService prod_service(prod_db);

// 测试环境（使用内存数据库）
SQLite test_db(":memory:", 1);
UserService test_service(test_db);
```

### 5. 移动语义

```cpp
// 移动构造
SQLite createTempDb() {
    SQLite temp(":memory:");
    temp.CreateTable("temp", {...});
    return temp;  // RVO 或移动
}

// 移动赋值
SQLite db1("db1.db");
SQLite db2("db2.db");
db2 = std::move(db1);  // db1 的资源转移给 db2
```

## 注意事项

### ⚠️ 重要提醒

1. **及时关闭不用的数据库**
   ```cpp
   {
       SQLite temp_db("temp.db");
       // ... 使用 ...
   }  // 作用域结束自动关闭
   
   // 或者显式关闭
   db.Shutdown();
   ```
   ```cpp
   {
       SQLite temp_db("temp.db");
       // ... 使用 ...
   }  // 作用域结束自动关闭
   
   // 或者显式关闭
   db.Shutdown();
   ```

3. **避免数据库文件冲突**
   ```cpp
   // ❌ 错误：多个实例访问同一文件
   SQLite db1("shared.db");
   SQLite db2("shared.db");  // 可能导致锁竞争
   
   // ✅ 正确：使用不同文件或明确设计为共享
   SQLite db1("db1.db");
   SQLite db2("db2.db");
   ```

## 性能考虑

### 连接池配置建议

```cpp
// 小型应用 - 少量连接
SQLite small_db("small.db", 2);

// 中型应用 - 适中连接数
SQLite medium_db("medium.db", 5);

// 大型应用 - 较多连接
SQLite large_db("large.db", 10);

// 临时/测试数据库 - 最小连接
SQLite temp_db(":memory:", 1);
```

### 内存占用

每个 SQLite 实例都会创建指定数量的连接池：
- 每个连接约占用 100KB-1MB（取决于 SQLite 配置）
- 建议根据实际需求调整连接池大小
- 不用的数据库及时销毁

## API 参考

### 构造函数

```cpp
// 默认构造（内存数据库）
SQLite db();

// 指定路径
SQLite db("path/to/database.db");

// 指定路径和连接池大小
SQLite db("path/to/database.db", 10);
```

### 移动操作

```cpp
// 移动构造
SQLite source("source.db");
SQLite target(std::move(source));

// 移动赋值
SQLite db1("db1.db");
SQLite db2("db2.db");
db2 = std::move(db1);
```

## 测试验证

运行多实例测试：

```bash
# 编译项目
cmake --build build

# 运行多实例测试
./build/bin/test_multi_instance
```

测试覆盖的场景：
- ✅ 多数据库并行操作
- ✅ 移动语义
- ✅ 工厂模式（智能指针管理）
- ✅ 多数据库事务

## 常见问题

### Q1: 多实例之间会互相干扰吗？

**A:** 不会。每个实例都有独立的连接池和资源，完全隔离。

### Q2: 可以同时打开多少个数据库？

**A:** 理论上没有限制，但受系统资源约束。建议根据实际需求合理设计。

### Q3: 内存数据库 (`:memory:`) 有什么用？

**A:** 
- 单元测试（快速、隔离）
- 临时数据存储
- 缓存层

### Q4: 如何优雅地管理多个数据库？

**A:** 推荐使用智能指针容器：
```cpp
std::map<std::string, std::unique_ptr<SQLite>> dbs;
```

## 总结

多实例架构为 SQLite 封装带来了：
- ✅ **更高的灵活性** - 支持复杂的应用场景
- ✅ **更好的可测试性** - 易于隔离和模拟
- ✅ **更清晰的代码** - 对象所有权明确
- ✅ **RAII 自动资源管理** - 简洁安全的编程方式
