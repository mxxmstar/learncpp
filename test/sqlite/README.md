# SQLite 封装库使用指南

## 概述

这是一个功能完整的 C++ SQLite 数据库封装库，提供以下特性：
- 连接池管理
- 参数化查询（防止 SQL 注入）
- RAII 风格的事务支持
- 流式 SQL 构建器
- 批量操作支持
- 丰富的辅助方法

## 快速开始

### 1. 初始化和关闭

```cpp
#include "sqlite/sqlite.h"

// 获取单例实例
auto& db = SQLite::GetInstance();

// 初始化（数据库路径，连接池大小）
db.Init("mydb.db", 5);

// 程序结束时关闭
db.Shutdown();
```

### 2. 基本操作

#### 创建表

```cpp
std::map<std::string, std::string> columns = {
    {"id", "INTEGER PRIMARY KEY AUTOINCREMENT"},
    {"name", "TEXT NOT NULL"},
    {"age", "INTEGER"},
    {"email", "TEXT UNIQUE"}
};

auto error = db.CreateTable("users", columns);
if (error.code != SQLite::ErrorCode::OK) {
    std::cout << "Error: " << error.message << std::endl;
}
```

#### 插入数据

```cpp
// 方式 1: 直接插入
auto error = db.Insert("users", {
    {"name", "'Alice'"},
    {"age", "25"},
    {"email", "'alice@example.com'"}
});

// 注意：字符串值需要用单引号包裹
```

#### 查询数据

```cpp
// 使用参数化查询
db.Query("SELECT * FROM users WHERE age > ?", {"18"}, [](void* stmt) {
    std::string name = SQLite::GetColumnText(stmt, 1);
    int age = SQLite::GetColumnInt(stmt, 2);
    std::string email = SQLite::GetColumnText(stmt, 3);
    
    std::cout << "Name: " << name 
              << ", Age: " << age 
              << ", Email: " << email << std::endl;
});
```

#### 更新数据

```cpp
auto error = db.Update("users", 
                      {{"age", "26"}},           // SET age = ?
                      "name = ?",                // WHERE name = ?
                      {"Alice"});                // 参数
```

#### 删除数据

```cpp
auto error = db.Delete("users", "age < ?", {"18"});
```

### 3. SQL Builder（流式 API）

SQL Builder 提供更优雅的 SQL 构建方式：

```cpp
SQLite::SQLBuilder builder;

// SELECT 查询
builder.Reset()
    .Select({"id", "name", "email"})
    .From("users")
    .WhereAnd("age > ?")
    .WhereAnd("status = ?")
    .OrderBy("name", true)      // true = ASC, false = DESC
    .Limit(10, 0)               // LIMIT 10 OFFSET 0
    .AddParam("18")
    .AddParam("active")
    .Query(db, [](void* stmt) {
        auto row = SQLite::GetRowMap(stmt);
        // 处理结果
    });

// INSERT
builder.Reset()
    .InsertInto("users")
    .Values({
        {"name", "'Bob'"},
        {"age", "30"},
        {"email", "'bob@example.com'"}
    })
    .Execute(db);

// UPDATE
builder.Reset()
    .Update("users")
    .Set({{"age", "31"}})
    .Where("name = ?")
    .AddParam("Bob")
    .Execute(db);

// DELETE
builder.Reset()
    .DeleteFrom("users")
    .Where("age < ?")
    .AddParam("18")
    .Execute(db);

// CREATE TABLE
builder.Reset()
    .CreateTable("products", true)  // true = IF NOT EXISTS
    .Column("id", "INTEGER", "PRIMARY KEY AUTOINCREMENT")
    .Column("name", "TEXT", "NOT NULL")
    .Column("price", "REAL", "DEFAULT 0.0")
    .Execute(db);
```

### 4. 事务处理

#### RAII 风格（推荐）

```cpp
{
    SQLite::Transaction trans(db);
    
    db.Insert("users", {{"name", "'Charlie'"}, {"age", "28"}});
    db.Insert("users", {{"name", "'David'"}, {"age", "32"}});
    
    // 提交事务
    trans.Commit();
}
// 如果没有调用 Commit()，析构函数会自动回滚
```

#### 手动控制

```cpp
db.BeginTransaction();

auto error = db.Execute("UPDATE users SET age = age + 1");
if (error.code != SQLite::ErrorCode::OK) {
    db.RollbackTransaction();
} else {
    db.CommitTransaction();
}
```

### 5. 批量操作

```cpp
// 批量插入（自动使用事务优化）
std::vector<std::string> columns = {"name", "age", "email"};
std::vector<std::vector<std::string>> rows = {
    {"'Eve'", "22", "'eve@example.com'"},
    {"'Frank'", "35", "'frank@example.com'"},
    {"'Grace'", "28", "'grace@example.com'"}
};

auto error = db.BatchInsert("users", columns, rows);
```

### 6. 辅助方法

#### 检查记录是否存在

```cpp
bool exists = false;
auto error = db.Exists("users", "email = ?", {"test@example.com"}, exists);
if (error.code == SQLite::ErrorCode::OK && exists) {
    std::cout << "User exists!" << std::endl;
}
```

#### 获取记录数

```cpp
int count = 0;
auto error = db.Count("users", "age > ?", {"25"}, count);
std::cout << "Count: " << count << std::endl;
```

#### 获取列值

```cpp
db.Query("SELECT * FROM users LIMIT 1", {}, [](void* stmt) {
    // 按索引获取（从 0 开始）
    std::string name = SQLite::GetColumnText(stmt, 1);
    int age = SQLite::GetColumnInt(stmt, 2);
    double salary = SQLite::GetColumnDouble(stmt, 3);
    
    // 或者获取整行作为 map
    auto row = SQLite::GetRowMap(stmt);
    for (const auto& [key, value] : row) {
        std::cout << key << ": " << value << std::endl;
    }
});
```

### 7. 错误处理

```cpp
auto error = db.Execute("SOME SQL");
if (error.code != SQLite::ErrorCode::OK) {
    std::cout << "Error code: " << static_cast<int>(error.code) << std::endl;
    std::cout << "Error message: " << error.message << std::endl;
    
    // 或者抛出异常
    throwIfError(error);
}
```

可用的错误码：
- `ErrorCode::OK` - 成功
- `ErrorCode::DATABASE_OPEN_FAILED` - 打开数据库失败
- `ErrorCode::PREPARE_STATEMENT_FAILED` - 准备语句失败
- `ErrorCode::EXECUTE_FAILED` - 执行失败
- `ErrorCode::QUERY_FAILED` - 查询失败
- `ErrorCode::INVALID_PARAMS` - 参数无效
- `ErrorCode::SHUTDOWN` - 数据库已关闭

## 最佳实践

### 1. 使用参数化查询防止 SQL 注入

```cpp
// ❌ 错误：字符串拼接
std::string sql = "SELECT * FROM users WHERE name = '" + userName + "'";

// ✅ 正确：使用参数
db.Query("SELECT * FROM users WHERE name = ?", {userName}, callback);
```

### 2. 使用事务提高批量操作性能

```cpp
// ❌ 慢：每条插入都单独提交
for (int i = 0; i < 1000; ++i) {
    db.Insert("table", values);
}

// ✅ 快：使用事务
SQLite::Transaction trans(db);
for (int i = 0; i < 1000; ++i) {
    db.Insert("table", values);
}
trans.Commit();

// 或使用 BatchInsert
db.BatchInsert("table", columns, rows);
```

### 3. 使用 SQL Builder 提高代码可读性

```cpp
// ❌ 难以阅读
std::string sql = "SELECT u.name, u.email FROM users u WHERE u.age > 18 AND u.status = 'active' ORDER BY u.name LIMIT 10";

// ✅ 清晰易懂
builder.Select({"u.name", "u.email"})
       .From("users u")
       .WhereAnd("u.age > ?")
       .WhereAnd("u.status = ?")
       .OrderBy("u.name")
       .Limit(10)
       .AddParam("18")
       .AddParam("active");
```

### 4. 及时关闭数据库连接

```cpp
// 在程序结束时调用
db.Shutdown();
```

## 运行测试

编译并运行测试：

```bash
# 构建项目
cmake --build .

# 运行单元测试
./bin/test_sqlite

# 运行示例
./bin/test_sqlite_example
```

## API 参考

### SQLite 类主要方法

| 方法 | 描述 |
|------|------|
| `GetInstance()` | 获取单例实例 |
| `Init(path, pool_size)` | 初始化数据库连接池 |
| `Shutdown()` | 关闭所有连接 |
| `Execute(sql)` | 执行无结果 SQL |
| `ExecuteWithParams(sql, params)` | 执行带参数的 SQL |
| `Query(sql, parser)` | 执行查询 |
| `QueryWithParams(sql, params, parser)` | 执行参数化查询 |
| `CreateTable(name, columns)` | 创建表 |
| `Insert(table, values)` | 插入记录 |
| `Update(table, values, where, params)` | 更新记录 |
| `Delete(table, where, params)` | 删除记录 |
| `Select(...)` | 条件查询 |
| `SelectAll(...)` | 全表查询 |
| `Exists(...)` | 检查记录存在 |
| `Count(...)` | 统计记录数 |
| `BatchInsert(...)` | 批量插入 |
| `BeginTransaction()` | 开始事务 |
| `CommitTransaction()` | 提交事务 |
| `RollbackTransaction()` | 回滚事务 |

### SQLBuilder 类主要方法

| 方法 | 描述 |
|------|------|
| `Select(columns)` | 构建 SELECT |
| `From(table)` | 指定表名 |
| `Where(condition)` | 设置 WHERE |
| `WhereAnd(condition)` | 添加 AND 条件 |
| `WhereOr(condition)` | 添加 OR 条件 |
| `OrderBy(column, asc)` | 排序 |
| `Limit(count, offset)` | 限制结果 |
| `InsertInto(table)` | 构建 INSERT |
| `Values(map)` | 设置插入值 |
| `Update(table)` | 构建 UPDATE |
| `Set(map)` | 设置更新值 |
| `DeleteFrom(table)` | 构建 DELETE |
| `CreateTable(table)` | 构建 CREATE TABLE |
| `Column(name, type, constraints)` | 添加列定义 |
| `AddParam(value)` | 添加参数 |
| `AddParams(values)` | 添加多个参数 |
| `GetSQL()` | 获取生成的 SQL |
| `GetParams()` | 获取参数列表 |
| `Execute(db)` | 执行 SQL |
| `Query(db, parser)` | 执行查询 |
| `Reset()` | 重置构建器 |

## 注意事项

1. **字符串值**：插入或更新时，字符串值需要用单引号包裹：`"'value'"`
2. **参数绑定**：使用 `?` 作为占位符，参数按顺序绑定
3. **线程安全**：通过连接池实现线程安全
4. **内存数据库**：使用 `":memory:"` 作为路径可创建内存数据库
5. **RAII 事务**：推荐使用 `Transaction` 类，自动管理事务生命周期
