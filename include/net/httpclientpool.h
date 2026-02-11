#pragma once
#include "net/httpclient.h"
#include <boost/pool/object_pool.hpp>
#include <boost/asio/io_context.hpp>
#include <string>
#include <memory>
#include <queue>
#include <vector>
#include <atomic>
class HttpClientPool {
public:
    struct Config {
        std::string host;
        uint16_t port = 80;
        std::size_t init_size = 5;
        std::size_t max_size = 20;
        int connect_timeout_ms = 30000;
        int idle_timeout_sec = 300;
    };
    
    static HttpClientPool& GetInstance();

    void Initialize(boost::asio::io_context& io_context, const Config& config);

    std::shared_ptr<PooledHttpCLient> GetHttpClient();

    void ReleaseHttpClient(std::shared_ptr<PooledHttpCLient> client);

    void Shutdown();

    struct PoolStats {
        std::size_t total;
        std::size_t available;
        std::size_t active;
        std::size_t created;
        std::size_t destroyed;
    };
    PoolStats GetStats() const;
private:
    HttpClientPool() = default;
    ~HttpClientPool();

    // 禁止拷贝
    HttpClientPool(const HttpClientPool&) = delete;
    HttpClientPool& operator=(const HttpClientPool&) = delete;

    ///@brief 创建连接
    AsioAsyncHttpClient* CreateHttpClient();

    ///@brief 销毁连接
    void DestroyHttpClient(AsioAsyncHttpClient* client);

    ///@brief 清理过期的连接
    void CleanupExpiredClients();
    
    struct PooledClient {
        AsioAsyncHttpClient* client;
        std::chrono::steady_clock::time_point last_used;
        std::size_t request_count;
        bool is_using;
        PooledClient() : client(nullptr), request_count(0), is_using(false) {}
    };
    
    boost::asio::io_context* io_context_ = nullptr;
    Config config_;
    mutable std::mutex mutex_;
    boost::object_pool<AsioAsyncHttpClient> client_pool_;
    std::queue<std::shared_ptr<PooledClient>> available_clients_;
    std::vector<std::shared_ptr<PooledClient>> all_clients;

    std::atomic<std::size_t> created_count_{ 0 };
    std::atomic<std::size_t> destoryed_count_{ 0 };
    std::atomic<bool> stopped_{ false };
};