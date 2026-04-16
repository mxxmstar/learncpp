#include "sqlite/connection_pool.h"
#include "log/logmanager.h"
#include <stdexcept>
#include <sstream>

// ============================================================================
// PooledConnection 实现
// ============================================================================

SQLiteConnectionPool::PooledConnection::PooledConnection(sqlite3* db, SQLiteConnectionPool& pool)
    : db_(db), pool_(&pool), released_(false) {
}

SQLiteConnectionPool::PooledConnection::~PooledConnection() {
    if (db_ && !released_) {
        pool_->release(db_);
        released_ = true;
    }
}

SQLiteConnectionPool::PooledConnection::PooledConnection(PooledConnection&& other) noexcept
    : db_(other.db_), pool_(other.pool_), released_(other.released_) {
    other.db_ = nullptr;
    other.pool_ = nullptr;
    other.released_ = true;
}

SQLiteConnectionPool::PooledConnection& SQLiteConnectionPool::PooledConnection::operator=(PooledConnection&& other) noexcept {
    if (this != &other) {
        // 释放当前连接
        if (db_ && !released_) {
            pool_->release(db_);
        }
        
        // 移动其他对象的资源
        db_ = other.db_;
        pool_ = other.pool_;
        released_ = other.released_;
        
        other.db_ = nullptr;
        other.pool_ = nullptr;
        other.released_ = true;
    }
    return *this;
}

// ============================================================================
// SQLiteConnectionPool 实现
// ============================================================================

SQLiteConnectionPool::SQLiteConnectionPool(const Config& config)
    : config_(config), last_health_check_(std::chrono::steady_clock::now()) {
    
    // 初始化最小连接数
    for (int i = 0; i < config_.min_connections; ++i) {
        sqlite3* db = createConnection();
        if (db) {
            idle_connections_.push(db);
            stats_.total_created++;
        }
    }
    
    stats_.total_connections = static_cast<int>(idle_connections_.size());
    stats_.idle_connections = stats_.total_connections;
    
    LOG_MAIN_INFO_AT("SQLiteConnectionPool initialized: min={}, max={}, path={}", 
                     config_.min_connections, config_.max_connections, config_.db_path);
}

SQLiteConnectionPool::~SQLiteConnectionPool() {
    shutdown();
}

sqlite3* SQLiteConnectionPool::createConnection() {
    sqlite3* db = nullptr;
    int rc = sqlite3_open(config_.db_path.c_str(), &db);
    
    if (rc != SQLITE_OK) {
        LOG_MAIN_ERROR_AT("Failed to create SQLite connection: {}", sqlite3_errmsg(db));
        if (db) {
            sqlite3_close(db);
        }
        return nullptr;
    }
    
    // 设置一些优化选项
    sqlite3_busy_timeout(db, 5000);  // 5秒忙等待超时
    
    LOG_MAIN_DEBUG_AT("Created new SQLite connection");
    return db;
}

void SQLiteConnectionPool::destroyConnection(sqlite3* db) {
    if (db) {
        sqlite3_close(db);
        LOG_MAIN_DEBUG_AT("Destroyed SQLite connection");
    }
}

bool SQLiteConnectionPool::isConnectionHealthy(sqlite3* db) {
    if (!db) return false;
    
    // 执行一个简单的查询来检查连接是否有效
    const char* test_sql = "SELECT 1";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, test_sql, -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        LOG_MAIN_WARN_AT("Connection health check failed: {}", sqlite3_errmsg(db));
        return false;
    }
    
    sqlite3_finalize(stmt);
    return true;
}

void SQLiteConnectionPool::cleanupIdleConnections() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto now = std::chrono::steady_clock::now();
    auto timeout = std::chrono::seconds(config_.idle_timeout_seconds);
    
    // 注意：queue 不支持遍历，所以我们只能清理到最小连接数
    // 如果需要更精细的控制，应该使用 list 或 vector
    while (static_cast<int>(idle_connections_.size()) > config_.min_connections) {
        sqlite3* db = idle_connections_.front();
        idle_connections_.pop();
        destroyConnection(db);
        
        total_count_--;
        stats_.total_connections--;
        stats_.idle_connections--;
        stats_.total_destroyed++;
        
        LOG_MAIN_DEBUG_AT("Cleaned up idle connection, remaining: {}", idle_connections_.size());
    }
}

SQLiteConnectionPool::PooledConnection SQLiteConnectionPool::acquire() {
    return acquireWithTimeout(config_.connection_timeout_ms);
}

bool SQLiteConnectionPool::tryAcquire(PooledConnection& conn) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (shutdown_.load()) {
        LOG_MAIN_WARN_AT("Connection pool is shutdown");
        return false;
    }
    
    if (idle_connections_.empty()) {
        // 尝试创建新连接
        if (total_count_.load() < config_.max_connections) {
            sqlite3* db = createConnection();
            if (db) {
                total_count_++;
                active_count_++;
                stats_.total_created++;
                stats_.total_connections++;
                stats_.active_connections++;
                
                {
                    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
                    stats_.total_acquired++;
                }
                
                conn = PooledConnection(db, *this);
                return true;
            }
        }
        return false;
    }
    
    // 从空闲队列获取连接
    sqlite3* db = idle_connections_.front();
    idle_connections_.pop();
    
    // 健康检查
    if (config_.enable_health_check && !isConnectionHealthy(db)) {
        LOG_MAIN_WARN_AT("Unhealthy connection detected, creating new one");
        destroyConnection(db);
        stats_.total_destroyed++;
        
        // 尝试创建新连接
        db = createConnection();
        if (!db) {
            return false;
        }
        stats_.total_created++;
    }
    
    active_count_++;
    stats_.active_connections++;
    stats_.idle_connections--;
    
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.total_acquired++;
    }
    
    conn = PooledConnection(db, *this);
    return true;
}

SQLiteConnectionPool::PooledConnection SQLiteConnectionPool::acquireWithTimeout(int timeout_ms) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    // 等待可用连接或超时
    bool acquired = cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this] {
        return !idle_connections_.empty() || shutdown_.load();
    });
    
    if (!acquired || shutdown_.load()) {
        stats_.timeout_count++;
        throw std::runtime_error("Connection acquisition timeout or pool shutdown");
    }
    
    // 从空闲队列获取连接
    sqlite3* db = idle_connections_.front();
    idle_connections_.pop();
    
    lock.unlock();
    
    // 健康检查
    if (config_.enable_health_check && !isConnectionHealthy(db)) {
        LOG_MAIN_WARN_AT("Unhealthy connection detected, creating new one");
        destroyConnection(db);
        stats_.total_destroyed++;
        
        db = createConnection();
        if (!db) {
            throw std::runtime_error("Failed to create healthy connection");
        }
        stats_.total_created++;
    }
    
    active_count_++;
    stats_.active_connections++;
    stats_.idle_connections--;
    
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.total_acquired++;
    }
    
    return PooledConnection(db, *this);
}

void SQLiteConnectionPool::release(sqlite3* db) {
    if (!db) return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (shutdown_.load()) {
        // 如果连接池已关闭，直接销毁连接
        destroyConnection(db);
        stats_.total_destroyed++;
    } else {
        // 检查是否超过最大连接数
        if (static_cast<int>(idle_connections_.size()) >= config_.max_connections) {
            destroyConnection(db);
            stats_.total_destroyed++;
        } else {
            // 返回空闲队列
            idle_connections_.push(db);
            stats_.idle_connections++;
            cv_.notify_one();  // 通知等待的线程
        }
    }
    
    active_count_--;
    stats_.active_connections--;
    
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.total_released++;
    }
}

void SQLiteConnectionPool::shutdown() {
    bool expected = false;
    if (!shutdown_.compare_exchange_strong(expected, true)) {
        return;  // 已经关闭
    }
    
    LOG_MAIN_INFO_AT("Shutting down SQLiteConnectionPool...");
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 关闭所有空闲连接
    while (!idle_connections_.empty()) {
        sqlite3* db = idle_connections_.front();
        idle_connections_.pop();
        destroyConnection(db);
        stats_.total_destroyed++;
    }
    
    stats_.total_connections = 0;
    stats_.idle_connections = 0;
    stats_.active_connections = 0;
    
    cv_.notify_all();
    
    logStats();
    LOG_MAIN_INFO_AT("SQLiteConnectionPool shutdown complete");
}

SQLiteConnectionPool::Stats SQLiteConnectionPool::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    Stats s = stats_;
    s.active_connections = active_count_.load();
    s.idle_connections = static_cast<int>(idle_connections_.size());
    s.total_connections = s.active_connections + s.idle_connections;
    return s;
}

void SQLiteConnectionPool::logStats() const {
    Stats s = getStats();
    
    LOG_MAIN_INFO_AT("=== SQLiteConnectionPool Statistics ===");
    LOG_MAIN_INFO_AT("Total Connections: {}", s.total_connections);
    LOG_MAIN_INFO_AT("Active Connections: {}", s.active_connections);
    LOG_MAIN_INFO_AT("Idle Connections: {}", s.idle_connections);
    LOG_MAIN_INFO_AT("Total Acquired: {}", s.total_acquired);
    LOG_MAIN_INFO_AT("Total Released: {}", s.total_released);
    LOG_MAIN_INFO_AT("Total Created: {}", s.total_created);
    LOG_MAIN_INFO_AT("Total Destroyed: {}", s.total_destroyed);
    LOG_MAIN_INFO_AT("Timeout Count: {}", s.timeout_count);
    LOG_MAIN_INFO_AT("=====================================");
}

void SQLiteConnectionPool::healthCheck() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - last_health_check_).count();
    
    if (elapsed < config_.health_check_interval_seconds) {
        return;  // 未到检查时间
    }
    
    last_health_check_ = now;
    
    LOG_MAIN_DEBUG_AT("Performing health check...");
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    int healthy_count = 0;
    int unhealthy_count = 0;
    
    // 检查所有空闲连接
    std::queue<sqlite3*> temp_queue;
    while (!idle_connections_.empty()) {
        sqlite3* db = idle_connections_.front();
        idle_connections_.pop();
        
        if (isConnectionHealthy(db)) {
            temp_queue.push(db);
            healthy_count++;
        } else {
            LOG_MAIN_WARN_AT("Removing unhealthy connection");
            destroyConnection(db);
            stats_.total_destroyed++;
            unhealthy_count++;
            
            // 创建新连接替换
            if (static_cast<int>(temp_queue.size()) < config_.min_connections) {
                sqlite3* new_db = createConnection();
                if (new_db) {
                    temp_queue.push(new_db);
                    stats_.total_created++;
                }
            }
        }
    }
    
    // 恢复队列
    idle_connections_ = std::move(temp_queue);
    
    LOG_MAIN_INFO_AT("Health check complete: {} healthy, {} unhealthy", 
                     healthy_count, unhealthy_count);
}
