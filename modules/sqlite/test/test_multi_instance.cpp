// SQLite 多实例测试
#include "sqlite/sqlite.h"
#include <iostream>
#include <cassert>
#include "log/logmanager.h"
#include <filesystem>


void test_multiple_instances() {
    std::cout << "\n=== Testing Multiple Database Instances ===" << std::endl;
    
    SQLite users_db("users.db", 5);      
    SQLite orders_db("orders.db", 5);
    SQLite logs_db("logs.db", 2);
    
    //表结构
    users_db.CreateTable("users", {
        {"id", "INTEGER PRIMARY KEY AUTOINCREMENT"},
        {"name", "TEXT NOT NULL"},
        {"email", "TEXT UNIQUE"}
    });
    
    orders_db.CreateTable("orders", {
        {"id", "INTEGER PRIMARY KEY AUTOINCREMENT"},
        {"user_id", "INTEGER"},
        {"product", "TEXT"},
        {"amount", "REAL"}
    });
    
    logs_db.CreateTable("logs", {
        {"id", "INTEGER PRIMARY KEY AUTOINCREMENT"},
        {"level", "TEXT"},
        {"message", "TEXT"},
        {"timestamp", "TEXT"}
    });
        
    users_db.Insert("users", {
        {"name", "Alice"},
        {"email", "alice@example.com"}
    });
    
    users_db.Insert("users", {
        {"name", "Bob"},
        {"email", "bob@example.com"}
    });
    
    orders_db.Insert("orders", {
        {"user_id", "1"},
        {"product", "Laptop"},
        {"amount", "999.99"}
    });
    
    orders_db.Insert("orders", {
        {"user_id", "2"},
        {"product", "Mouse"},
        {"amount", "29.99"}
    });
    
    logs_db.Insert("logs", {
        {"level", "INFO"},
        {"message", "User Alice logged in"},
        {"timestamp", "2026-03-25 10:00:00"}
    });
        
    std::cout << "\n--- Users Database ---" << std::endl;
    users_db.Query("SELECT * FROM users", [](void* stmt) {
        std::cout << "User: " << SQLite::GetColumnText(stmt, 1) 
                  << ", Email: " << SQLite::GetColumnText(stmt, 2) << std::endl;
    });
    
    std::cout << "\n--- Orders Database ---" << std::endl;
    orders_db.Query("SELECT * FROM orders", [](void* stmt) {
        std::cout << "Order: " << SQLite::GetColumnText(stmt, 2) 
                  << " - " << SQLite::GetColumnText(stmt, 3) 
                  << " ($" << SQLite::GetColumnDouble(stmt, 4) << ")" << std::endl;
    });
    
    std::cout << "\n--- Logs Database ---" << std::endl;
    logs_db.Query("SELECT * FROM logs", [](void* stmt) {
        std::cout << "[" << SQLite::GetColumnText(stmt, 1) << "] " 
                  << SQLite::GetColumnText(stmt, 2) 
                  << " at " << SQLite::GetColumnText(stmt, 3) << std::endl;
    });
    
    // 统计记录数
    int user_count = 0, order_count = 0, log_count = 0;
    users_db.Count("users", "", {}, user_count);
    orders_db.Count("orders", "", {}, order_count);
    logs_db.Count("logs", "", {}, log_count);
    
    std::cout << "\n--- Statistics ---" << std::endl;
    std::cout << "Total users: " << user_count << std::endl;
    std::cout << "Total orders: " << order_count << std::endl;
    std::cout << "Total logs: " << log_count << std::endl;
}

void test_move_semantics() {
    std::cout << "\n=== Testing Move Semantics ===" << std::endl;
    std::remove("temp1.db");

    SQLite db1("temp1.db", 3);
    db1.CreateTable("test", {{"id", "INTEGER"}});
    db1.Insert("test", {{"id", "1"}});
        
    SQLite db2 = std::move(db1);
        
    db2.Insert("test", {{"id", "2"}});
    
    int count = 0;
    db2.Count("test", "", {}, count);
    std::cout << "Records in moved database: " << count << std::endl;
    assert(count == 2);
}

void test_factory_pattern() {
    std::cout << "\n=== Testing Factory-like Pattern ===" << std::endl;
    
    std::map<std::string, std::unique_ptr<SQLite>> databases;
    
    databases["main"] = std::make_unique<SQLite>("main.db", 5);
    databases["cache"] = std::make_unique<SQLite>("cache.db", 3);
    databases["analytics"] = std::make_unique<SQLite>("analytics.db", 2);
    
    databases["main"]->CreateTable("data", {
        {"key", "TEXT PRIMARY KEY"},
        {"value", "TEXT"}
    });
    
    databases["cache"]->CreateTable("cache", {
        {"key", "TEXT PRIMARY KEY"},
        {"value", "TEXT"},
        {"expire", "INTEGER"}
    });
    
    // 插入数据
    databases["main"]->Insert("data", {
        {"key", "config"},
        {"value", "production"}
    });
    
    databases["cache"]->Insert("cache", {
        {"key", "user_1"},
        {"value", "Alice"},
        {"expire", "3600"}
    });
    
    // 查询数据
    std::cout << "--- Main Database ---" << std::endl;
    databases["main"]->Query("SELECT * FROM data", [](void* stmt) {
        std::cout << "Key: " << SQLite::GetColumnText(stmt, 0) 
                  << ", Value: " << SQLite::GetColumnText(stmt, 1) << std::endl;
    });
    
    std::cout << "--- Cache Database ---" << std::endl;
    databases["cache"]->Query("SELECT * FROM cache", [](void* stmt) {
        std::cout << "Key: " << SQLite::GetColumnText(stmt, 0) 
                  << ", Value: " << SQLite::GetColumnText(stmt, 1)
                  << ", Expire: " << SQLite::GetColumnInt(stmt, 2) << std::endl;
    });
}

void test_mixed_usage() {
    std::cout << "\n=== Testing Mixed Usage (Multiple Instances) ===" << std::endl;
    std::remove("direct.db");
    // 内存数据库 ，每次连接看到的是不同的数据库
    SQLite singleton_db(":memory:", 2);
    // singleton_db 已自动初始化
    
    SQLite direct_db("direct.db", 3);
    
    // 创建表
    singleton_db.CreateTable("singleton_table", {{"id", "INTEGER"}});
    direct_db.CreateTable("direct_table", {{"id", "INTEGER"}});
    
    singleton_db.Insert("singleton_table", {{"id", "1"}});
    direct_db.Insert("direct_table", {{"id", "100"}});
    
    int singleton_count = 0, direct_count = 0;
    singleton_db.Count("singleton_table", "", {}, singleton_count);
    direct_db.Count("direct_table", "", {}, direct_count);
    
    std::cout << "Singleton DB records: " << singleton_count << std::endl;
    std::cout << "Direct DB records: " << direct_count << std::endl;
    assert(singleton_count != 1);   // 内存数据库中每次连接看到的是不同的数据库
    assert(direct_count == 1);
}

void test_transaction_with_multiple_dbs() {
    std::cout << "\n=== Testing Transactions with Multiple Databases ===" << std::endl;
    
    SQLite db1("db1.db", 3);
    SQLite db2("db2.db", 3);
    
    db1.CreateTable("table1", {{"value", "INTEGER"}});
    db2.CreateTable("table2", {{"value", "INTEGER"}});
    
    // 插入数据
    {
        SQLite::Transaction trans1(db1);
        db1.Insert("table1", {{"value", "1"}});
        db1.Insert("table1", {{"value", "2"}});
        trans1.Commit();
    }
    
    {
        SQLite::Transaction trans2(db2);
        db2.Insert("table2", {{"value", "100"}});
        db2.Insert("table2", {{"value", "200"}});
        trans2.Commit();
    }
    
    // 查询数据
    int count1 = 0, count2 = 0;
    db1.Count("table1", "", {}, count1);
    db2.Count("table2", "", {}, count2);
    
    std::cout << "DB1 records: " << count1 << std::endl;
    std::cout << "DB2 records: " << count2 << std::endl;
    assert(count1 == 2 && count2 == 2);
}

void RunMultiInstanceTests() {
    std::cout << "========================================" << std::endl;
    std::cout << "SQLite Multi-Instance Feature Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    
    try {
        test_multiple_instances();
        test_move_semantics();
        test_factory_pattern();
        test_mixed_usage();
        test_transaction_with_multiple_dbs();
        
        std::cout << "\n锟?All multi-instance tests passed!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "\n锟?Test failed with exception: " << e.what() << std::endl;
    }
}

int main() {
    LogManager& log_manager = LogManager::getInstance();
    log_manager.Init();
    RunMultiInstanceTests();
    return 0;
}
