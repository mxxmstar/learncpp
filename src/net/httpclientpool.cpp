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

void PooledClient::PostJson(const std::string& url, const boost::json::object& req_obj,
    HandlerFactory handler_factory, int timeout_ms) {
    if (client && handler_factory) {
        auto base_handler = handler_factory();
        client->PostJson(url, req_obj, std::move(base_handler), timeout_ms);
    }
}

void PooledClient::GetJson(const std::string& url, HandlerFactory handler_factory,
    int timeout_ms) {
    if (client && handler_factory) {
        auto base_handler = handler_factory();
        client->GetJson(url, std::move(base_handler), timeout_ms);
    }
}

void PooledClient::PostJsonWithHandler(const std::string& url, const boost::json::object& req_obj,
    HttpClientPool::CompleteHandler handler, int timeout_ms) {
    if (client) {
        client->PostJson(url, req_obj, std::move(handler), timeout_ms);
    }
}

void PooledClient::GetJsonWithHandler(const std::string& url, HttpClientPool::CompleteHandler handler,
    int timeout_ms) {
    if (client) {
        client->GetJson(url, std::move(handler), timeout_ms);
    }
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
    
    // 设置默认的 Handler 工厂（空 handler）
    handler_factory_ = []() -> CompleteHandler {
        return CompleteHandler();  // 默认构造，空的 handler
    };

    for (std::size_t i = 0; i < config_.init_size; ++i) {
        if (auto client = CreatePooledClient()) {
            available_clients_.push_back(client);
            all_clients_.insert(client);
        }
    }
    LOG_MAIN_INFO_AT("HttpClientPool initialized with {} clients", config_.init_size);
}

void HttpClientPool::SetHandlerFactory(HandlerFactory factory) {
    std::lock_guard<std::mutex> lock(mutex_);
    handler_factory_ = std::move(factory);
}

HttpClientPool::HandlerFactory HttpClientPool::GetHandlerFactory() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return handler_factory_;
}

void HttpClientPool::AddHandlerDecorator(HandlerDecorator decorator) {
    std::lock_guard<std::mutex> lock(mutex_);
    decorators_.push_back(std::move(decorator));
}

HttpClientPool::CompleteHandler HttpClientPool::CreateDecoratedHandler(CompleteHandler base_handler) {
    // 应用所有装饰器，从内到外包裹
    CompleteHandler decorated = std::move(base_handler);
    
    // 反向遍历，确保装饰器按添加顺序生效
    for (auto it = decorators_.rbegin(); it != decorators_.rend(); ++it) {
        auto original = std::move(decorated);
        (*it)(original);  // 装饰器修改 original
        decorated = std::move(original);
    }
    
    return decorated;
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
        // 使用原子操作，无需锁保护
        pooled_client->in_use.store(true);
        pooled_client->request_count.fetch_add(1);
        LOG_MAIN_DEBUG_AT("Client acquired, request_count: {}", pooled_client->request_count.load());
    }

    return pooled_client;
}

PooledClientGuard HttpClientPool::AcquireGuard() {
    auto client = Acquire();
    if (client) {
        return PooledClientGuard(client, this);
    }
    // 返回空的 guard
    return PooledClientGuard(nullptr, nullptr);
}

void HttpClientPool::Release(std::shared_ptr<PooledClient> client) {
    if (!client) return;

    std::lock_guard<std::mutex> lock(mutex_);
    if (stopped_) return;

    ReleaseInternal(client);
}

void HttpClientPool::ReleaseInternal(std::shared_ptr<PooledClient> client) {
    if (!client) return;

    // 使用原子操作，无需锁保护
    client->in_use.store(false);
    client->last_used = std::chrono::steady_clock::now();

    if (client->request_count.load() >= config_.max_requests_per_client) {
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
    std::size_t current_created = created_count_.load() - destroyed_count_.load();
    return {
        all_clients_.size(),
        available_clients_.size(),
        all_clients_.size() - available_clients_.size(),
        current_created,
        created_count_.load(),
        destroyed_count_.load()
    };
}

std::shared_ptr<PooledClient> HttpClientPool::CreatePooledClient() {
    try {
        if (!io_context_) {
            LOG_MAIN_ERROR_AT("CreatePooledClient failed: io_context_ is null");
            return nullptr;
        }
        
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
    if (!client || !client->IsValid() || client->in_use.load()) return false;
    auto idle = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - client->last_used).count();
    return idle > config_.idle_timeout_sec;
}

}
