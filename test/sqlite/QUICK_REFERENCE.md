# SQLite 快速参考手册

## 初始化与关闭

```cpp
auto& db = SQLite::GetInstance();
db.Init("database.db", 5);  // 路径，连接池大小
// ... 使用数据库 ...
db.Shutdown();
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
