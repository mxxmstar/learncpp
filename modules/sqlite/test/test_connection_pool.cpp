#include "sqlite/connection_pool.h"
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include "common/log/logmanager.h"
void testBasicUsage() {
    std::cout << "\n=== Test 1: Basic Usage ===" << std::endl;
    
    SQLiteConnectionPool::Config config;
    config.db_path = "test_pool.db";
    config.min_connections = 3;
    config.max_connections = 10;
    
    SQLiteConnectionPool pool(config);
    
    // 获取连接
    auto conn = pool.acquire();
    
    // 执行 SQL
    const char* sql = "CREATE TABLE IF NOT EXISTS users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER)";
    char* errmsg = nullptr;
    int rc = sqlite3_exec(conn.get(), sql, nullptr, nullptr, &errmsg);
    
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << errmsg << std::endl;
        sqlite3_free(errmsg);
    } else {
        std::cout << "Table created successfully" << std::endl;
    }
    
    // 连接会自动释放（RAII）
    std::cout << "Connection released automatically" << std::endl;
    
    pool.logStats();
}

void testConcurrentAccess() {
    std::cout << "\n=== Test 2: Concurrent Access ===" << std::endl;
    
    SQLiteConnectionPool::Config config;
    config.db_path = "test_concurrent.db";
    config.min_connections = 5;
    config.max_connections = 15;
    config.connection_timeout_ms = 10000;  // 增加超时时间
    
    SQLiteConnectionPool pool(config);
    
    // 创建表
    {
        auto conn = pool.acquire();
        const char* sql = "CREATE TABLE IF NOT EXISTS data (id INTEGER PRIMARY KEY, value TEXT)";
        sqlite3_exec(conn.get(), sql, nullptr, nullptr, nullptr);
        
        // 启用 WAL 模式以支持更好的并发
        sqlite3_exec(conn.get(), "PRAGMA journal_mode=WAL", nullptr, nullptr, nullptr);
        sqlite3_exec(conn.get(), "PRAGMA busy_timeout=5000", nullptr, nullptr, nullptr);
    }
    
    // 多线程并发访问
    const int thread_count = 10;
    const int ops_per_thread = 50;  // 减少操作数
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    std::atomic<int> error_count{0};
    
    auto start = std::chrono::steady_clock::now();
    
    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back([&pool, i, ops_per_thread, &success_count, &error_count]() {
            for (int j = 0; j < ops_per_thread; ++j) {
                try {
                    auto conn = pool.acquireWithTimeout(5000);  // 5秒超时
                    
                    // 使用事务批量插入，减少锁竞争
                    sqlite3_exec(conn.get(), "BEGIN IMMEDIATE", nullptr, nullptr, nullptr);
                    
                    std::string sql = "INSERT INTO data (value) VALUES ('thread_" + 
                                     std::to_string(i) + "_op_" + std::to_string(j) + "')";
                    
                    char* errmsg = nullptr;
                    int rc = sqlite3_exec(conn.get(), sql.c_str(), nullptr, nullptr, &errmsg);
                    
                    if (rc == SQLITE_OK) {
                        sqlite3_exec(conn.get(), "COMMIT", nullptr, nullptr, nullptr);
                        success_count++;
                    } else {
                        sqlite3_exec(conn.get(), "ROLLBACK", nullptr, nullptr, nullptr);
                        if (errmsg) {
                            // 只在第一次打印错误，避免刷屏
                            static std::atomic<int> print_count{0};
                            if (print_count++ < 5) {
                                std::cerr << "Thread " << i << " error: " << errmsg << std::endl;
                            }
                            sqlite3_free(errmsg);
                        }
                        error_count++;
                        
                        // 短暂等待后重试
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                        j--;  // 重试当前操作
                    }
                    
                } catch (const std::exception& e) {
                    static std::atomic<int> print_count{0};
                    if (print_count++ < 5) {
                        std::cerr << "Thread " << i << " exception: " << e.what() << std::endl;
                    }
                    error_count++;
                    
                    // 短暂等待
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    j--;  // 重试
                }
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }
    
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    std::cout << "Completed " << success_count << " successful operations" << std::endl;
    std::cout << "Failed " << error_count << " operations (retried)" << std::endl;
    std::cout << "Time: " << duration << "ms" << std::endl;
    if (duration > 0) {
        std::cout << "Throughput: " << (success_count * 1000.0 / duration) 
                  << " ops/sec" << std::endl;
    }
    
    pool.logStats();
}

void testTimeout() {
    std::cout << "\n=== Test 3: Timeout Handling ===" << std::endl;
    
    SQLiteConnectionPool::Config config;
    config.db_path = "test_timeout.db";
    config.min_connections = 1;
    config.max_connections = 1;  // 只允许1个连接
    config.connection_timeout_ms = 1000;  // 1秒超时
    
    SQLiteConnectionPool pool(config);
    
    // 获取唯一的连接并保持
    auto conn1 = pool.acquire();
    std::cout << "First connection acquired" << std::endl;
    
    // 尝试获取第二个连接（应该超时）
    try {
        auto conn2 = pool.acquireWithTimeout(1000);
        std::cerr << "ERROR: Should have timed out!" << std::endl;
    } catch (const std::runtime_error& e) {
        std::cout << "Expected timeout: " << e.what() << std::endl;
    }
    
    // 释放第一个连接
    conn1.~PooledConnection();
    
    // 现在应该可以获取了
    auto conn3 = pool.acquire();
    std::cout << "Successfully acquired connection after release" << std::endl;
    
    pool.logStats();
}

void testTryAcquire() {
    std::cout << "\n=== Test 4: Try Acquire ===" << std::endl;
    
    SQLiteConnectionPool::Config config;
    config.db_path = "test_try.db";
    config.min_connections = 1;
    config.max_connections = 1;
    
    SQLiteConnectionPool pool(config);
    
    // 获取唯一的连接
    auto conn1 = pool.acquire();
    
    // 尝试获取第二个连接（非阻塞）
    SQLiteConnectionPool::PooledConnection conn2;
    if (pool.tryAcquire(conn2)) {
        std::cout << "Unexpectedly acquired connection" << std::endl;
    } else {
        std::cout << "Correctly failed to acquire (no blocking)" << std::endl;
    }
    
    pool.logStats();
}

void testHealthCheck() {
    std::cout << "\n=== Test 5: Health Check ===" << std::endl;
    
    SQLiteConnectionPool::Config config;
    config.db_path = "test_health.db";
    config.min_connections = 3;
    config.max_connections = 5;
    config.enable_health_check = true;
    config.health_check_interval_seconds = 1;  // 1秒检查一次（用于测试）
    
    SQLiteConnectionPool pool(config);
    
    // 执行一些操作
    {
        auto conn = pool.acquire();
        const char* sql = "CREATE TABLE IF NOT EXISTS test (id INTEGER PRIMARY KEY)";
        sqlite3_exec(conn.get(), sql, nullptr, nullptr, nullptr);
    }
    
    // 手动触发健康检查
    pool.healthCheck();
    
    pool.logStats();
}

void testMoveSemantics() {
    std::cout << "\n=== Test 6: Move Semantics ===" << std::endl;
    
    SQLiteConnectionPool::Config config;
    config.db_path = "test_move.db";
    config.min_connections = 2;
    
    SQLiteConnectionPool pool(config);
    
    // 测试 PooledConnection 的移动语义
    auto conn1 = pool.acquire();
    std::cout << "Connection 1 acquired" << std::endl;
    
    // 移动构造
    auto conn2 = std::move(conn1);
    std::cout << "Connection moved (conn1 is now invalid)" << std::endl;
    
    // 移动赋值
    auto conn3 = pool.acquire();
    conn3 = std::move(conn2);
    std::cout << "Connection move-assigned" << std::endl;
    
    pool.logStats();
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "SQLite Connection Pool Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;
    LogManager& log_manager = LogManager::getInstance();
    log_manager.Init();
    try {
        testBasicUsage();
        testConcurrentAccess();
        testTimeout();
        testTryAcquire();
        testHealthCheck();
        testMoveSemantics();
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "All tests completed successfully!" << std::endl;
        std::cout << "========================================" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "\nTest failed with exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
