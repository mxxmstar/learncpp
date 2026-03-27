# SQLite 快速参考手册

## 创建数据库实例

### 直接构造（推荐 - 支持多数据库）

```cpp
// 创建多个独立的数据库实例
SQLite db1("database1.db", 5);   // 路径，连接池大小
SQLite db2("database2.db", 3);
SQLite db3(":memory:", 1);       // 内存数据库

// 使用智能指针管理
auto db = std::make_unique<SQLite>("mydb.db");

// 移动语义
SQLite source("source.db");
SQLite target = std::move(source);
```

## 多数据库使用场景

### 场景 1：主从架构

```cpp
SQLite primary("primary.db", 10);   // 主库 - 写操作
SQLite replica("replica.db", 5);    // 从库 - 读操作

primary.Insert("users", {...});     // 写入主库
replica.Query("SELECT * FROM users"); // 从从库读取
```

### 场景 2：功能分离

```cpp
SQLite config_db("config.db", 2);   // 配置数据库
SQLite data_db("data.db", 5);       // 数据数据库
SQLite cache_db("cache.db", 3);     // 缓存数据库

config_db.Insert("settings", {...});
data_db.Query("SELECT * FROM records");
cache_db.Delete("expired", "...");
```

### 场景 3：微服务

```cpp
std::map<std::string, std::unique_ptr<SQLite>> services;
services["user"] = std::make_unique<SQLite>("user.db");
services["order"] = std::make_unique<SQLite>("order.db");
services["product"] = std::make_unique<SQLite>("product.db");

services["user"]->Insert(...);
services["order"]->Query(...);
```

## 基本 CRUD 操作

### 创建表
```cpp
db.CreateTable("table_name", {
    {"id", "INTEGER PRIMARY KEY"},
    {"name", "TEXT NOT NULL"},
    {"value", "REAL"}
});
```

### 插入
```cpp
db.Insert("table", {
    {"column1", "'value1'"},  // 字符串加单引号
    {"column2", "123"}        // 数字不需要
});
```

### 查询
```cpp
db.Query("SELECT * FROM table WHERE id > ?", {"10"}, [](void* stmt) {
    auto name = SQLite::GetColumnText(stmt, 0);
    auto age = SQLite::GetColumnInt(stmt, 1);
    auto value = SQLite::GetColumnDouble(stmt, 2);
    auto row = SQLite::GetRowMap(stmt);  // 获取整行
});
```

### 更新
```cpp
db.Update("table", 
          {{"column", "new_value"}}, 
          "id = ?", 
          {"123"});
```

### 删除
```cpp
db.Delete("table", "id = ?", {"123"});
```

## SQL Builder（流式 API）

### SELECT
```cpp
SQLite::SQLBuilder builder;
builder.Select({"col1", "col2"})
       .From("table")
       .WhereAnd("age > ?")
       .WhereOr("status = ?")
       .OrderBy("name", true)   // ASC
       .Limit(10, 0)            // LIMIT, OFFSET
       .AddParam("18")
       .AddParam("active")
       .Query(db, callback);
```

### INSERT
```cpp
builder.Reset()
       .InsertInto("table")
       .Values({{"name", "'test'"}, {"value", "100"}})
       .Execute(db);
```

### UPDATE
```cpp
builder.Reset()
       .Update("table")
       .Set({{"column", "value"}})
       .Where("id = ?")
       .AddParam("123")
       .Execute(db);
```

### DELETE
```cpp
builder.Reset()
       .DeleteFrom("table")
       .Where("id = ?")
       .AddParam("123")
       .Execute(db);
```

### CREATE TABLE
```cpp
builder.Reset()
       .CreateTable("table", true)  // IF NOT EXISTS
       .Column("id", "INTEGER", "PRIMARY KEY")
       .Column("name", "TEXT", "NOT NULL")
       .Execute(db);
```

## 事务处理

### RAII 方式（推荐）
```cpp
{
    SQLite::Transaction trans(db);
    db.Insert(...);
    db.Insert(...);
    trans.Commit();  // 必须显式调用
}
```

### 手动控制
```cpp
db.BeginTransaction();
// ... 操作 ...
if (success)
    db.CommitTransaction();
else
    db.RollbackTransaction();
```

## 批量操作

```cpp
std::vector<std::string> columns = {"name", "age"};
std::vector<std::vector<std::string>> rows = {
    {"'Alice'", "25"},
    {"'Bob'", "30"}
};
db.BatchInsert("users", columns, rows);
```

## 辅助方法

### 检查存在
```cpp
bool exists;
db.Exists("table", "id = ?", {"123"}, exists);
```

### 计数
```cpp
int count;
db.Count("table", "age > ?", {"18"}, count);
```

### 获取列值
```cpp
db.Query("SELECT * FROM table", {}, [](void* stmt) {
    auto text = SQLite::GetColumnText(stmt, index);
    auto num = SQLite::GetColumnInt(stmt, index);
    auto dbl = SQLite::GetColumnDouble(stmt, index);
    auto map = SQLite::GetRowMap(stmt);
});
```

## 错误处理

```cpp
auto error = db.Execute(sql);
if (error.code != SQLite::ErrorCode::OK) {
    std::cerr << error.message << std::endl;
    throwIfError(error);  // 或抛出异常
}
```

## 注意事项

1. **字符串值**：必须用单引号包裹 `"'value'"`
2. **参数占位符**：使用 `?`，按顺序绑定
3. **内存数据库**：使用 `":memory:"` 作为路径
4. **线程安全**：通过连接池实现
5. **及时关闭**：程序结束时调用 `Shutdown()`
