#pragma once
#include "net/httpclient.h"
#include <boost/asio/io_context.hpp>
#include <string>
#include <memory>
#include <deque>
#include <unordered_set>
#include <atomic>

class PooledHttpClient;

class HttpClientPool {
public:
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
        std::size_t total;
        std::size_t available;
        std::size_t active;
        std::size_t created;
        std::size_t destroyed;
    };

    static HttpClientPool& GetInstance();

    void Init(boost::asio::io_context& io_context, const Config& config);
    std::shared_ptr<PooledHttpClient> Acquire();
    void Stop();
    PoolStats GetStats() const;

private:
    /**
     * @brief 池化客户端, 包含一个异步http客户端和一些状态信息
     */
    struct PooledClient {
        std::shared_ptr<AsioAsyncHttpClient> client;
        std::chrono::steady_clock::time_point last_used;
        std::size_t request_count = 0;
        bool in_use = false;
    };

    HttpClientPool() = default;
    ~HttpClientPool();
    HttpClientPool(const HttpClientPool&) = delete;
    HttpClientPool& operator=(const HttpClientPool&) = delete;

    std::shared_ptr<PooledClient> CreatePooledClient();
    void ReleaseClient(std::shared_ptr<PooledClient> client);
    void CleanupExpiredClients();
    bool IsClientExpired(const std::shared_ptr<PooledClient>& client) const;

    boost::asio::io_context* io_context_ = nullptr;
    Config config_;
    mutable std::mutex mutex_;  // 保护池化客户端的访问
    std::deque<std::shared_ptr<PooledClient>> available_clients_;   // 可用客户端
    std::unordered_set<std::shared_ptr<PooledClient>> all_clients_;   // 所有客户端

    std::atomic<std::size_t> created_count_{0};
    std::atomic<std::size_t> destroyed_count_{0};
    std::atomic<bool> stopped_{false};

    friend class PooledHttpClient;
};

/// @brief 
class PooledHttpClient {
public:
    using CompleteHandler = std::function<void(bool success, const boost::json::object& rsp_obj)>;

    PooledHttpClient(std::shared_ptr<HttpClientPool::PooledClient> client, HttpClientPool* pool);
    ~PooledHttpClient();

    void PostJson(const std::string& url, const boost::json::object& req_obj, CompleteHandler handler, int timeout_ms = 5000);
    void GetJson(const std::string& url, CompleteHandler handler, int timeout_ms = 5000);
    bool IsValid() const;
    std::size_t GetRequestCount() const;

private:
    void Release();

    std::shared_ptr<HttpClientPool::PooledClient> client_;
    HttpClientPool* pool_ = nullptr;
    bool released_ = false;
};
