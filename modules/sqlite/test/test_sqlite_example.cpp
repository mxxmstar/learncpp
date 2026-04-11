// SQLite 类和 SQLBuilder 类使用示例
#include "sqlite/sqlite.h"
#include <iostream>
#include <string>
#include "log/logmanager.h"
void test_basic_usage() {
    // 直接构造数据库实例
    SQLite db("test.db", 5);  // 5 个连接池
    
    // 创建表
    std::map<std::string, std::string> columns = {
        {"id", "INTEGER PRIMARY KEY AUTOINCREMENT"},
        {"name", "TEXT NOT NULL"},
        {"age", "INTEGER"},
        {"email", "TEXT UNIQUE"}
    };
    
    auto error = db.CreateTable("users", columns);
    if (error.code != SQLite::ErrorCode::OK) {
        std::cout << "Create table failed: " << error.message << std::endl;
        return;
    }
    
    // 插入数据
    error = db.Insert("users", {
        {"name", "'Alice'"},
        {"age", "25"},
        {"email", "'alice@example.com'"}
    });
    
    // 查询数据
    db.QueryWithParams("SELECT * FROM users WHERE age > ?", {"18"}, [](void* stmt) {
        std::string name = SQLite::GetColumnText(stmt, 1);
        int age = SQLite::GetColumnInt(stmt, 2);
        std::cout << "Name: " << name << ", Age: " << age << std::endl;
    });
    
    // 更新数据
    error = db.Update("users", 
                     {{"age", "26"}}, 
                     "name = ?", 
                     {"Alice"});
    
    // 删除数据
    error = db.Delete("users", "age < ?", {"18"});
    
    // 不需要手动 Shutdown()，析构函数会自动调用
}

void test_sql_builder() {
    SQLite db("test.db", 5);
    
    // 使用 SQLBuilder 构建 SELECT 查询
    SQLite::SQLBuilder builder;
    auto error = builder
        .Select({"id", "name", "email"})
        .From("users")
        .WhereAnd("age > ?")
        .WhereAnd("status = ?")
        .OrderBy("name", true)
        .Limit(10, 0)
        .AddParam("18")
        .AddParam("active")
        .Query(db, [](void* stmt) {
            std::cout << "User: " << SQLite::GetColumnText(stmt, 1) << std::endl;
        });
    
    // 使用 SQLBuilder 插入数据
    builder.Reset();
    builder.InsertInto("users")
          .Values({
              {"name", "'Bob'"},
              {"age", "30"},
              {"email", "'bob@example.com'"}
          });
    error = builder.Execute(db);
    
    // 使用 SQLBuilder 更新数据
    builder.Reset();
    builder.Update("users")
          .Set({{"age", "31"}})
          .Where("name = ?")
          .AddParam("Bob");
    error = builder.Execute(db);
    
    // 使用 SQLBuilder 删除数据
    builder.Reset();
    builder.DeleteFrom("users")
          .Where("age < ?")
          .AddParam("18");
    error = builder.Execute(db);
    
    // 使用 SQLBuilder 创建表
    builder.Reset();
    builder.CreateTable("products", true)
        .Column("id", "INTEGER", "PRIMARY KEY AUTOINCREMENT")
        .Column("name", "TEXT", "NOT NULL")
        .Column("price", "REAL", "DEFAULT 0.0")
        .Execute(db);
    
    db.Shutdown();
}

void test_transaction() {
    SQLite db("test.db", 5);
    
    // 使用 RAII 风格的事务
    {
        SQLite::Transaction trans(db);
        
        // 插入多条记录
        db.Insert("users", {{"name", "'Charlie'"}, {"age", "28"}});
        db.Insert("users", {{"name", "'David'"}, {"age", "32"}});
        
        // 提交事务
        trans.Commit();
    }
    
    // 或者手动控制事务
    db.BeginTransaction();
    
    auto error = db.Execute("UPDATE users SET age = age + 1");
    if (error.code != SQLite::ErrorCode::OK) {
        db.RollbackTransaction();
        std::cout << "Transaction rolled back: " << error.message << std::endl;
    } else {
        db.CommitTransaction();
        std::cout << "Transaction committed successfully" << std::endl;
    }
    
    db.Shutdown();
}

void test_batch_operations() {
    SQLite db("test.db", 5);
    
    // 批量插入（使用事务优化性能）
    std::vector<std::string> columns = {"name", "age", "email"};
    std::vector<std::vector<std::string>> rows = {
        {"'Eve'", "22", "'eve@example.com'"},
        {"'Frank'", "35", "'frank@example.com'"},
        {"'Grace'", "28", "'grace@example.com'"}
    };
    
    auto error = db.BatchInsert("users", columns, rows);
    if (error.code == SQLite::ErrorCode::OK) {
        std::cout << "Batch insert successful" << std::endl;
    }
    
    db.Shutdown();
}

void test_helper_methods() {
    SQLite db("test.db", 5);
    
    // 检查记录是否存在
    bool exists = false;
    auto error = db.Exists("users", "email = ?", {"alice@example.com"}, exists);
    if (error.code == SQLite::ErrorCode::OK) {
        std::cout << "User exists: " << (exists ? "yes" : "no") << std::endl;
    }
    
    // 获取记录数
    int count = 0;
    error = db.Count("users", "age > ?", {"25"}, count);
    if (error.code == SQLite::ErrorCode::OK) {
        std::cout << "User count (age > 25): " << count << std::endl;
    }
    
    // 使用辅助方法获取数据
    db.Query("SELECT * FROM users LIMIT 5", [](void* stmt) {
        auto row = SQLite::GetRowMap(stmt);
        for (const auto& [key, value] : row) {
            std::cout << key << ": " << value << "  ";
        }
        std::cout << std::endl;
    });
    
    // 不需要手动 Shutdown()，析构函数会自动调用
}

int main() {
    LogManager& log_manager = LogManager::getInstance();
    log_manager.Init();
    std::cout << "=== Testing Basic Usage ===" << std::endl;
    test_basic_usage();
    
    std::cout << "\n=== Testing SQL Builder ===" << std::endl;
    test_sql_builder();
    
    std::cout << "\n=== Testing Transaction ===" << std::endl;
    test_transaction();
    
    std::cout << "\n=== Testing Batch Operations ===" << std::endl;
    test_batch_operations();
    
    std::cout << "\n=== Testing Helper Methods ===" << std::endl;
    test_helper_methods();
    
    return 0;
}
