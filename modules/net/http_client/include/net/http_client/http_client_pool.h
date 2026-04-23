#pragma once
#include "net/http_client/http_client.h"
#include <boost/asio/io_context.hpp>
#include <string>
#include <memory>
#include <deque>
#include <unordered_set>
#include <atomic>

namespace Net {

// 前向声明
class HttpClientPool;

/// @brief Handler 工厂函数类型（提前定义，供 PooledClient 使用）
using HandlerFactory = std::function<AsioAsyncHttpClient::CompleteHandler()>;

/**
 * @brief 池化客户端，包含一个异步 http 客户端和一些状态信息
 */
struct PooledClient {
    std::shared_ptr<AsioAsyncHttpClient> client;
    std::chrono::steady_clock::time_point last_used;
    std::atomic<std::size_t> request_count{0};  // 改为原子类型，支持线程安全访问
    std::atomic<bool> in_use{false};            // 改为原子类型，支持线程安全访问
    
    /// @brief 检查客户端是否有效
    bool IsValid() const { return client != nullptr; }
    
    /// @brief 获取请求次数
    std::size_t GetRequestCount() const { return request_count.load(); }
    
    void PostJson(const std::string& url, const boost::json::object& req_obj, 
                  AsioAsyncHttpClient::CompleteHandler handler, int timeout_ms = 5000);
    void GetJson(const std::string& url, AsioAsyncHttpClient::CompleteHandler handler, 
                 int timeout_ms = 5000);
    
    /// @brief 使用 Handler 工厂发起 POST请求（自动应用装饰器）
    void PostJson(const std::string& url, const boost::json::object& req_obj,
                  HandlerFactory handler_factory, int timeout_ms = 5000);
    
    /// @brief 使用 Handler 工厂发起 GET 请求（自动应用装饰器）
    void GetJson(const std::string& url, HandlerFactory handler_factory,
                 int timeout_ms = 5000);
    
    /// @brief 直接指定 handler 发起请求（不应用装饰器）
    void PostJsonWithHandler(const std::string& url, const boost::json::object& req_obj,
                            AsioAsyncHttpClient::CompleteHandler handler, int timeout_ms = 5000);
    void GetJsonWithHandler(const std::string& url, AsioAsyncHttpClient::CompleteHandler handler,
                           int timeout_ms = 5000);
};


class PooledClientGuard;
class HttpClientPool {
public:
    // 回调处理器类型
    using CompleteHandler = AsioAsyncHttpClient::CompleteHandler;
    
    /// @brief Handler 装饰器：可以在调用原始 handler 前后添加额外逻辑
    using HandlerDecorator = std::function<void(CompleteHandler& original_handler)>;
    
    /// @brief Handler 工厂函数类型
    using HandlerFactory = std::function<CompleteHandler()>;
    
    struct Config {
        std::string host;
        uint16_t port = 80;
        std::size_t init_size = 5;
        std::size_t max_size = 20;
        int connect_timeout_ms = 30000;
        int idle_timeout_sec = 300;
        std::size_t max_requests_per_client = 100;
    };

    struct PoolStats {
        std::size_t total;          ///< 当前池中客户端总数
        std::size_t available;      ///< 可用客户端数
        std::size_t active;         ///< 正在使用的客户端数
        std::size_t current_created;   ///< 当前已创建数量（created - destroyed）
        std::size_t lifetime_created;  ///< 历史累计创建数量
        std::size_t lifetime_destroyed;///< 历史累计销毁数量
    };

    /// @brief 默认构造函数
    HttpClientPool() = default;
    
    /// @brief 析构函数
    ~HttpClientPool();
    
    /// @brief 禁用拷贝构造和赋值
    HttpClientPool(const HttpClientPool&) = delete;
    HttpClientPool& operator=(const HttpClientPool&) = delete;
    
    /// @brief 初始化连接池
    void Init(boost::asio::io_context& io_context, const Config& config);
    
    /// @brief 设置全局的 Handler 工厂
    void SetHandlerFactory(HandlerFactory factory);
    HandlerFactory GetHandlerFactory() const;
    
    /// @brief 添加全局的 Handler 装饰器（在所有 handler 外层包装一层逻辑）
    /// 可以用于日志、统计等横切关注点
    void AddHandlerDecorator(HandlerDecorator decorator);
    
    /// @brief 创建一个新的 handler，应用所有装饰器
    CompleteHandler CreateDecoratedHandler(CompleteHandler base_handler);
    
    /// @brief 获取客户端（返回 shared_ptr，需要手动 Release）
    /// @details 主要用在 长连接/需要延迟释放/作为类成员 的场景
    std::shared_ptr<PooledClient> Acquire();
    
    /// @brief 归还客户端到池中
    void Release(std::shared_ptr<PooledClient> client);
    
    /// @brief 获取客户端（返回 RAII 守卫，自动释放）
    /// @details 主要用在 异步回调/临时作用域/异常 中，确保客户端自动归还到池中
    PooledClientGuard AcquireGuard();
    
    void Stop();
    PoolStats GetStats() const;
    
private:

    std::shared_ptr<PooledClient> CreatePooledClient();
    void ReleaseInternal(std::shared_ptr<PooledClient> client);  // 内部释放逻辑
    void CleanupExpiredClients();
    bool IsClientExpired(const std::shared_ptr<PooledClient>& client) const;

    boost::asio::io_context* io_context_ = nullptr;
    Config config_;
    mutable std::mutex mutex_;  // 保护池化客户端的访问
    std::deque<std::shared_ptr<PooledClient>> available_clients_;   // 可用客户端
    std::unordered_set<std::shared_ptr<PooledClient>> all_clients_;   // 所有客户端
    
    HandlerFactory handler_factory_;  ///< 全局的 Handler 工厂
    std::vector<HandlerDecorator> decorators_;  ///< Handler 装饰器链
    
    std::atomic<std::size_t> created_count_{0};
    std::atomic<std::size_t> destroyed_count_{0};
    std::atomic<bool> stopped_{false};
    
};

/**
 * @brief RAII 风格的客户端守卫，确保客户端使用后自动归还到池中
 *
 * 用法示例：
 * @code
 * auto client_guard = pool.AcquireGuard();
 * if (client_guard) {
 *     client_guard->GetJson(url, [guard = std::move(client_guard)](bool success, auto& rsp) {
 *         // guard 析构时会自动释放客户端
 *     });
 * }
 * @endcode
 */
class PooledClientGuard {
public:
    explicit PooledClientGuard(std::shared_ptr<PooledClient> client, HttpClientPool* pool)
        : client_(std::move(client)), pool_(pool) {
    }

    ~PooledClientGuard() {
        reset();
    }

    // 禁止拷贝
    PooledClientGuard(const PooledClientGuard&) = delete;
    PooledClientGuard& operator=(const PooledClientGuard&) = delete;

    // 允许移动
    PooledClientGuard(PooledClientGuard&& other) noexcept
        : client_(std::move(other.client_)), pool_(other.pool_) {
        other.pool_ = nullptr;
    }

    PooledClientGuard& operator=(PooledClientGuard&& other) noexcept {
        if (this != &other) {
            reset();
            client_ = std::move(other.client_);
            pool_ = other.pool_;
            other.pool_ = nullptr;
        }
        return *this;
    }

    PooledClient* get() const { return client_.get(); }
    PooledClient& operator*() const { return *client_; }
    PooledClient* operator->() const { return client_.get(); }
    explicit operator bool() const { return client_ != nullptr; }

    /// @brief 获取底层 shared_ptr（用于需要捕获的场景）
    std::shared_ptr<PooledClient> getShared() const { return client_; }

    /// @brief 手动释放客户端（提前归还到池中）
    void reset() {
        if (client_ && pool_) {
            pool_->Release(client_);
            pool_ = nullptr;
        }
        client_.reset();
    }

private:
    std::shared_ptr<PooledClient> client_;
    HttpClientPool* pool_ = nullptr;
};

}
