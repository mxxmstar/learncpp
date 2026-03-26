#include "net/httpclientpool.h"
#include "net/httpclient.h"
#include "log/logmanager.h"
#include <stdexcept>

namespace Net {

void PooledClient::PostJson(const std::string& url, const boost::json::object& req_obj,
    AsioAsyncHttpClient::CompleteHandler handler, int timeout_ms) {
    if (client) {
        client->PostJson(url, req_obj, std::move(handler), timeout_ms);
    }
}

void PooledClient::GetJson(const std::string& url, AsioAsyncHttpClient::CompleteHandler handler,
    int timeout_ms) {
    if (client) {
        client->GetJson(url, std::move(handler), timeout_ms);
    }
}

HttpClientPool& HttpClientPool::GetInstance() {
    static HttpClientPool instance;
    return instance;
}

HttpClientPool::~HttpClientPool() {
    Stop();
}

void HttpClientPool::Init(boost::asio::io_context& io_context, const Config& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (io_context_ && !stopped_) {
        throw std::runtime_error("HttpClientPool already initialized");
    }

    io_context_ = &io_context;
    config_ = config;
    stopped_ = false;

    for (std::size_t i = 0; i < config_.init_size; ++i) {
        if (auto client = CreatePooledClient()) {
            available_clients_.push_back(client);
            all_clients_.insert(client);
        }
    }
    LOG_MAIN_INFO_AT("HttpClientPool initialized with {} clients", config_.init_size);
}

std::shared_ptr<PooledClient> HttpClientPool::Acquire() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stopped_) {
        LOG_MAIN_ERROR_AT("HttpClientPool is stopped");
        return nullptr;
    }

    CleanupExpiredClients();

    std::shared_ptr<PooledClient> pooled_client;

    if (!available_clients_.empty()) {
        pooled_client = available_clients_.front();
        available_clients_.pop_front();
    }
    else if (all_clients_.size() < config_.max_size) {
        pooled_client = CreatePooledClient();
        if (pooled_client) {
            all_clients_.insert(pooled_client);
        }
    }
    else {
        LOG_MAIN_WARN_AT("HttpClientPool exhausted, max_size: {}", config_.max_size);
        return nullptr;
    }

    if (pooled_client) {
        pooled_client->in_use = true;
        pooled_client->request_count++;
        LOG_MAIN_DEBUG_AT("Client acquired, request_count: {}", pooled_client->request_count);
    }

    return pooled_client;
}

void HttpClientPool::Release(std::shared_ptr<PooledClient> client) {
    if (!client) return;

    std::lock_guard<std::mutex> lock(mutex_);
    if (stopped_) return;

    client->in_use = false;
    client->last_used = std::chrono::steady_clock::now();

    if (client->request_count >= config_.max_requests_per_client) {
        LOG_MAIN_DEBUG_AT("Client reached max requests, destroying");
        all_clients_.erase(client);
        ++destroyed_count_;
    }
    else {
        available_clients_.push_back(client);
    }
}    

void HttpClientPool::Stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stopped_) return;
    stopped_ = true;

    available_clients_.clear();
    for (auto& client : all_clients_) {
        if (client->client) {
            ++destroyed_count_;
        }
    }
    all_clients_.clear();
    LOG_MAIN_INFO_AT("HttpClientPool stopped");
}

HttpClientPool::PoolStats HttpClientPool::GetStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return {
        all_clients_.size(),
        available_clients_.size(),
        all_clients_.size() - available_clients_.size(),
        created_count_.load(),
        destroyed_count_.load()
    };
}

std::shared_ptr<PooledClient> HttpClientPool::CreatePooledClient() {
    try {
        auto pooled = std::make_shared<PooledClient>();
        pooled->client = std::make_shared<AsioAsyncHttpClient>(*io_context_, config_.host, config_.port);
        pooled->last_used = std::chrono::steady_clock::now();
        ++created_count_;
        LOG_MAIN_DEBUG_AT("Client created, total: {}", created_count_.load());
        return pooled;
    }
    catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("CreatePooledClient failed: {}", e.what());
        return nullptr;
    }
}

void HttpClientPool::ReturnClient(std::shared_ptr<PooledClient> client) {
    if (!client) return;

    std::lock_guard<std::mutex> lock(mutex_);
    if (stopped_) return;

    client->in_use = false;
    client->last_used = std::chrono::steady_clock::now();

    if (client->request_count >= config_.max_requests_per_client) {
        LOG_MAIN_DEBUG_AT("Client reached max requests, destroying");
        all_clients_.erase(client);
        ++destroyed_count_;
    }
    else {
        available_clients_.push_back(client);
    }
}

void HttpClientPool::CleanupExpiredClients() {
    auto it = available_clients_.begin();
    while (it != available_clients_.end()) {
        if (IsClientExpired(*it)) {
            all_clients_.erase(*it);
            ++destroyed_count_;
            it = available_clients_.erase(it);
        }
        else {
            ++it;
        }
    }
}

bool HttpClientPool::IsClientExpired(const std::shared_ptr<PooledClient>& client) const {
    if (!client || client->in_use) return false;
    auto idle = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - client->last_used).count();
    return idle > config_.idle_timeout_sec;
}

}
