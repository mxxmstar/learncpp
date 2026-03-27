// SQLite 新用法示例 - 推荐使用直接构造方式
#include "sqlite/sqlite.h"
#include <iostream>
#include "log/logmanager.h"
// ============================================================
// 推荐用法：直接构造（RAII 自动管理）
// ============================================================
void recommended_usage() {
    std::cout << "=== New Style (Multi-Instance) ===" << std::endl;
    
    // 1. 直接构造 - 自动初始化
    SQLite db("new_style.db", 5);
    // 不需要手动调用 Init()
    
    // 2. 使用数据库
    db.CreateTable("users", {
        {"id", "INTEGER PRIMARY KEY"},
        {"name", "TEXT"}
    });
    
    db.Insert("users", {{"name", "'Bob'"}});
    
    // 3. 析构函数自动关闭（不会忘记）
    // 作用域结束时自动调用 Shutdown()
    
    std::cout << "Recommended usage completed\n" << std::endl;
}

// ============================================================
// 新用法优势 1：支持多数据库
// ============================================================
void multi_database_usage() {
    std::cout << "=== Multi-Database Usage ===" << std::endl;
    
    // 创建多个独立的数据库
    SQLite users_db("users.db", 5);
    SQLite orders_db("orders.db", 5);
    SQLite logs_db("logs.db", 2);
    
    // 各数据库独立操作
    users_db.CreateTable("users", {{"id", "INTEGER"}, {"name", "TEXT"}});
    orders_db.CreateTable("orders", {{"id", "INTEGER"}, {"product", "TEXT"}});
    logs_db.CreateTable("logs", {{"id", "INTEGER"}, {"message", "TEXT"}});
    
    // 并行插入数据
    users_db.Insert("users", {{"name", "'Alice'"}});
    orders_db.Insert("orders", {{"product", "'Laptop'"}});
    logs_db.Insert("logs", {{"message", "'User created'"}});
    
    // 分别查询
    int user_count, order_count, log_count;
    users_db.Count("users", "", {}, user_count);
    orders_db.Count("orders", "", {}, order_count);
    logs_db.Count("logs", "", {}, log_count);
    
    std::cout << "Users: " << user_count 
              << ", Orders: " << order_count 
              << ", Logs: " << log_count << std::endl;
    
    std::cout << "Multi-database usage completed\n" << std::endl;
}

// ============================================================
// 新用法优势 2：依赖注入，易于测试
// ============================================================

// 业务类
class UserService {
    SQLite& db_;
public:
    explicit UserService(SQLite& db) : db_(db) {}
    
    void createUser(const std::string& name) {
        db_.Insert("users", {{"name", "'" + name + "'"}});
    }
    
    int getUserCount() {
        int count = 0;
        db_.Count("users", "", {}, count);
        return count;
    }
};

void production_usage() {
    std::cout << "=== Production Usage ===" << std::endl;
    
    // 生产环境：使用真实数据库
    SQLite prod_db("production.db", 5);
    prod_db.CreateTable("users", {{"id", "INTEGER"}, {"name", "TEXT"}});
    
    UserService prod_service(prod_db);
    prod_service.createUser("Charlie");
    
    std::cout << "Production users: " << prod_service.getUserCount() << std::endl;
    std::cout << "Production usage completed\n" << std::endl;
}

void test_usage() {
    std::cout << "=== Test Usage ===" << std::endl;
    
    // 测试环境：使用内存数据库（快速、隔离）
    SQLite test_db(":memory:", 1);
    test_db.CreateTable("users", {{"id", "INTEGER"}, {"name", "TEXT"}});
    
    UserService test_service(test_db);
    test_service.createUser("TestUser1");
    test_service.createUser("TestUser2");
    
    std::cout << "Test users: " << test_service.getUserCount() << std::endl;
    std::cout << "Test usage completed\n" << std::endl;
}

// ============================================================
// 新用法优势 3：移动语义，灵活的所有权转移
// ============================================================
void move_semantics_usage() {
    std::cout << "=== Move Semantics Usage ===" << std::endl;
    
    // 创建临时数据库
    SQLite temp_db("temp.db", 2);
    temp_db.CreateTable("data", {{"value", "INTEGER"}});
    temp_db.Insert("data", {{"value", "100"}});
    
    // 移动所有权（资源转移）
    SQLite target_db = std::move(temp_db);
    
    // temp_db 现在为空，target_db 拥有资源
    target_db.Insert("data", {{"value", "200"}});
    
    int count = 0;
    target_db.Count("data", "", {}, count);
    std::cout << "Moved database records: " << count << std::endl;
    
    std::cout << "Move semantics usage completed\n" << std::endl;
}

// ============================================================
// 新用法优势 4：智能指针管理
// ============================================================
void smart_pointer_usage() {
    std::cout << "=== Smart Pointer Usage ===" << std::endl;
    
    // 使用 unique_ptr 管理数据库
    auto db1 = std::make_unique<SQLite>("dynamic1.db");
    auto db2 = std::make_unique<SQLite>("dynamic2.db");
    
    db1->CreateTable("table1", {{"id", "INTEGER"}});
    db2->CreateTable("table2", {{"id", "INTEGER"}});
    
    // 动态创建和销毁
    db1->Insert("table1", {{"id", "1"}});
    db2->Insert("table2", {{"id", "2"}});
    
    // 作用域结束自动销毁
    
    std::cout << "Smart pointer usage completed\n" << std::endl;
}

// ============================================================
// 混合用法（过渡期可以共存）
// ============================================================
void mixed_usage() {
    std::cout << "=== Mixed Usage (Multiple Instances) ===" << std::endl;
    
    // 使用不同的数据库实例
    SQLite singleton_db(":memory:", 2);
    singleton_db.CreateTable("singleton_table", {{"id", "INTEGER"}});
    
    // 新方式（推荐使用）
    SQLite instance_db("instance.db", 3);
    instance_db.CreateTable("instance_table", {{"id", "INTEGER"}});
    
    // 两个独立的数据库，互不干扰
    singleton_db.Insert("singleton_table", {{"id", "1"}});
    instance_db.Insert("instance_table", {{"id", "100"}});
    
    int singleton_count, instance_count;
    singleton_db.Count("singleton_table", "", {}, singleton_count);
    instance_db.Count("instance_table", "", {}, instance_count);
    
    std::cout << "Singleton table: " << singleton_count << " records" << std::endl;
    std::cout << "Instance table: " << instance_count << " records" << std::endl;
    
    std::cout << "Mixed usage completed\n" << std::endl;
}

// ============================================================
// 主函数 - 运行所有示例
// ============================================================
int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "SQLite Usage Comparison Examples" << std::endl;
    std::cout << "========================================\n" << std::endl;
    LogManager& log_manager = LogManager::getInstance();
    log_manager.Init();
    try {
        // 推荐用法
        recommended_usage();
        
        // 新用法的各种优势场景
        multi_database_usage();
        production_usage();
        test_usage();
        move_semantics_usage();
        smart_pointer_usage();
        mixed_usage();
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "✓ All examples completed successfully!" << std::endl;
        std::cout << "========================================" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
