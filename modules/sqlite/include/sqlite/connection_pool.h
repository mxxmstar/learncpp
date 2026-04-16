#pragma once

#include <string>
#include <memory>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <chrono>
#include <sqlite3.h>

/**
 * @brief SQLite 连接池
 * 
 * 提供线程安全的 SQLite 数据库连接池管理，支持：
 * - 连接复用
 * - 自动扩容
 * - 超时控制
 * - 健康检查
 * - 统计信息
 */
class SQLiteConnectionPool {
public:
    /**
     * @brief 连接池配置
     */
    struct Config {
        std::string db_path = ":memory:";           ///< 数据库路径
        int min_connections = 5;                     ///< 最小连接数
        int max_connections = 20;                    ///< 最大连接数
        int idle_timeout_seconds = 300;              ///< 空闲连接超时时间（秒）
        int connection_timeout_ms = 5000;            ///< 获取连接超时时间（毫秒）
        bool enable_health_check = true;             ///< 是否启用健康检查
        int health_check_interval_seconds = 60;      ///< 健康检查间隔（秒）
    };
    
    /**
     * @brief 连接池统计信息
     */
    struct Stats {
        int total_connections = 0;                   ///< 总连接数
        int active_connections = 0;                  ///< 活跃连接数
        int idle_connections = 0;                    ///< 空闲连接数
        int total_acquired = 0;                      ///< 累计获取连接次数
        int total_released = 0;                      ///< 累计释放连接次数
        int total_created = 0;                       ///< 累计创建连接次数
        int total_destroyed = 0;                     ///< 累计销毁连接次数
        int timeout_count = 0;                       ///< 超时次数
    };
    
    /**
     * @brief 包装的数据库连接（RAII）
     */
    class PooledConnection {
    public:
        PooledConnection() : db_(nullptr), pool_(nullptr), released_(true) {}
        PooledConnection(sqlite3* db, SQLiteConnectionPool& pool);
        ~PooledConnection();
        
        // 禁止拷贝
        PooledConnection(const PooledConnection&) = delete;
        PooledConnection& operator=(const PooledConnection&) = delete;
        
        // 允许移动
        PooledConnection(PooledConnection&& other) noexcept;
        PooledConnection& operator=(PooledConnection&& other) noexcept;
        
        /**
         * @brief 获取原始 sqlite3 指针
         */
        sqlite3* get() const { return db_; }
        
        /**
         * @brief 转换为 bool，检查连接是否有效
         */
        explicit operator bool() const { return db_ != nullptr; }
        
        /**
         * @brief 箭头操作符
         */
        sqlite3* operator->() const { return db_; }
        
    private:
        sqlite3* db_ = nullptr;
        SQLiteConnectionPool* pool_ = nullptr;
        bool released_ = false;
    };
    
    /**
     * @brief 构造函数
     * @param config 连接池配置
     */
    explicit SQLiteConnectionPool(const Config& config = Config());
    
    /**
     * @brief 析构函数
     */
    ~SQLiteConnectionPool();
    
    // 禁止拷贝
    SQLiteConnectionPool(const SQLiteConnectionPool&) = delete;
    SQLiteConnectionPool& operator=(const SQLiteConnectionPool&) = delete;
    
    /**
     * @brief 从连接池获取一个连接（阻塞直到可用或超时）
     * @return PooledConnection RAII 包装的连接
     * @throws std::runtime_error 如果超时或无法创建连接
     */
    PooledConnection acquire();
    
    /**
     * @brief 尝试获取连接（非阻塞）
     * @param conn 输出参数，成功时填充
     * @return true 成功, false 失败（无可用连接）
     */
    bool tryAcquire(PooledConnection& conn);
    
    /**
     * @brief 带超时的获取连接
     * @param timeout_ms 超时时间（毫秒）
     * @return PooledConnection RAII 包装的连接
     * @throws std::runtime_error 如果超时
     */
    PooledConnection acquireWithTimeout(int timeout_ms);
    
    /**
     * @brief 释放连接回连接池（由 PooledConnection 自动调用）
     */
    void release(sqlite3* db);
    
    /**
     * @brief 关闭连接池，释放所有连接
     */
    void shutdown();
    
    /**
     * @brief 检查连接池是否已关闭
     */
    bool isShutdown() const { return shutdown_.load(); }
    
    /**
     * @brief 获取当前统计信息
     */
    Stats getStats() const;
    
    /**
     * @brief 打印统计信息到日志
     */
    void logStats() const;
    
    /**
     * @brief 执行健康检查
     */
    void healthCheck();
    
    /**
     * @brief 获取配置
     */
    const Config& getConfig() const { return config_; }
    
private:
    /**
     * @brief 创建新的数据库连接
     */
    sqlite3* createConnection();
    
    /**
     * @brief 销毁数据库连接
     */
    void destroyConnection(sqlite3* db);
    
    /**
     * @brief 检查连接是否健康
     */
    bool isConnectionHealthy(sqlite3* db);
    
    /**
     * @brief 清理超时的空闲连接
     */
    void cleanupIdleConnections();
    
    Config config_;
    
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<sqlite3*> idle_connections_;
    
    std::atomic<int> active_count_{0};
    std::atomic<int> total_count_{0};
    std::atomic<bool> shutdown_{false};
    
    // 统计信息
    mutable std::mutex stats_mutex_;
    Stats stats_;
    
    // 健康检查
    std::chrono::steady_clock::time_point last_health_check_;
};
