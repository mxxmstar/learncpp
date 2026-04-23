#include "net/http_client/http_client_pool.h"
#include "log/logmanager.h"
#include <boost/json.hpp>
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
using namespace Net;
void TestBasicUsage() {
    std::cout << "\n=== Test Basic Usage ===" << std::endl;

    boost::asio::io_context io_context;

    HttpClientPool::Config config;
    config.host = "httpbin.org";
    config.port = 80;
    config.init_size = 2;
    config.max_size = 5;

    // 创建连接池实例（不再使用单例）
    HttpClientPool pool;
    pool.Init(io_context, config);

    // 使用 RAII 守卫，自动释放客户端
    auto client_guard = pool.AcquireGuard();
    if (!client_guard) {
        std::cout << "Failed to acquire client" << std::endl;
        return;
    }

    std::cout << "Client acquired, valid: " << client_guard->IsValid() << std::endl;

    boost::json::object req_obj;
    req_obj["test"] = "hello";

    // 直接传递 handler
    client_guard->PostJsonWithHandler("/post", req_obj, [](bool success, const boost::json::object& rsp) {
        if (success) {
            std::cout << "POST success" << std::endl;
            if (rsp.contains("json")) {
                std::cout << "json: " << rsp.at("json") << std::endl;
            }
        } else {
            std::cout << "POST failed" << std::endl;
        }
    });

    io_context.run();
    pool.Stop();
}

void TestPoolStats() {
    std::cout << "\n=== Test Pool Stats ===" << std::endl;

    boost::asio::io_context io_context;

    HttpClientPool::Config config;
    config.host = "httpbin.org";
    config.port = 80;
    config.init_size = 3;
    config.max_size = 5;

    // 创建连接池实例（不再使用单例）
    HttpClientPool pool;
    pool.Init(io_context, config);

    // 初始状态检查
    auto stats = pool.GetStats();
    std::cout << "Initial stats: total=" << stats.total
              << ", available=" << stats.available
              << ", active=" << stats.active
              << ", current_created=" << stats.current_created
              << ", lifetime_created=" << stats.lifetime_created
              << ", lifetime_destroyed=" << stats.lifetime_destroyed << std::endl;

    std::vector<std::shared_ptr<PooledClient>> clients;
    for (int i = 0; i < 4; ++i) {
        auto client = pool.Acquire();
        if (client) {
            clients.push_back(client);
            std::cout << "Acquired client " << i << ", request_count: "
                      << client->GetRequestCount() << std::endl;
        }
    }

    // 获取 4 个客户端后的状态
    stats = pool.GetStats();
    std::cout << "After acquire 4: total=" << stats.total
              << ", available=" << stats.available
              << ", active=" << stats.active << std::endl;

    // 释放所有客户端
    std::cout << "Releasing all clients..." << std::endl;
    for (auto& c : clients) {
        pool.Release(c);
    }
    clients.clear();

    // 释放后的状态
    stats = pool.GetStats();
    std::cout << "After release: total=" << stats.total
              << ", available=" << stats.available
              << ", active=" << stats.active
              << ", current_created=" << stats.current_created
              << ", lifetime_created=" << stats.lifetime_created
              << ", lifetime_destroyed=" << stats.lifetime_destroyed << std::endl;

    pool.Stop();
}

void TestConcurrentAccess() {
    std::cout << "\n=== Test Concurrent Access ===" << std::endl;

    boost::asio::io_context io_context;
    auto work = boost::asio::make_work_guard(io_context);

    std::thread io_thread([&io_context]() {
        io_context.run();
    });

    HttpClientPool::Config config;
    config.host = "httpbin.org";
    config.port = 80;
    config.init_size = 2;
    config.max_size = 10;

    // 创建连接池实例（不再使用单例）
    HttpClientPool pool;
    pool.Init(io_context, config);

    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    std::atomic<int> fail_count{0};

    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&pool, &success_count, &fail_count, i]() {
            // 使用 RAII 守卫，自动释放客户端
            auto client_guard = pool.AcquireGuard();
            if (!client_guard) {
                std::cout << "Thread " << i << " failed to acquire client" << std::endl;
                fail_count++;
                return;
            }

            boost::json::object req_obj;
            req_obj["thread_id"] = i;

            // 直接传递 handler
            client_guard->PostJsonWithHandler("/post", req_obj, [&success_count, &fail_count, i](bool success, const boost::json::object& rsp) {
                if (success) {
                    std::cout << "Thread " << i << " request success" << std::endl;
                    success_count++;
                } else {
                    std::cout << "Thread " << i << " request failed" << std::endl;
                    fail_count++;
                }
            });
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    std::this_thread::sleep_for(std::chrono::seconds(3));

    work.reset();
    io_thread.join();

    auto stats = pool.GetStats();
    std::cout << "Final stats: total=" << stats.total
              << ", available=" << stats.available
              << ", current_created=" << stats.current_created
              << ", lifetime_created=" << stats.lifetime_created
              << ", lifetime_destroyed=" << stats.lifetime_destroyed << std::endl;
    std::cout << "Success: " << success_count << ", Failed: " << fail_count << std::endl;

    pool.Stop();
}

void TestMaxRequests() {
    std::cout << "\n=== Test Max Requests Per Client ===" << std::endl;

    boost::asio::io_context io_context;

    HttpClientPool::Config config;
    config.host = "httpbin.org";
    config.port = 80;
    config.init_size = 1;
    config.max_size = 2;
    config.max_requests_per_client = 3;

    // 创建连接池实例（不再使用单例）
    HttpClientPool pool;
    pool.Init(io_context, config);

    for (int i = 0; i < 5; ++i) {
        auto stats = pool.GetStats();
        std::cout << "Request " << i << ": total=" << stats.total
                  << ", current_created=" << stats.current_created
                  << ", lifetime_destroyed=" << stats.lifetime_destroyed << std::endl;

        auto client = pool.Acquire();
        if (client) {
            std::cout << "Client request_count: " << client->GetRequestCount() << std::endl;
            // 使用后立即释放
            pool.Release(client);
        }
    }

    auto stats = pool.GetStats();
    std::cout << "Final: current_created=" << stats.current_created
              << ", lifetime_destroyed=" << stats.lifetime_destroyed << std::endl;

    pool.Stop();
}

/// @brief 测试客户端有效性检查
void TestClientValidity() {
    std::cout << "\n=== Test Client Validity ===" << std::endl;

    boost::asio::io_context io_context;

    HttpClientPool::Config config;
    config.host = "httpbin.org";
    config.port = 80;
    config.init_size = 2;
    config.max_size = 3;

    // 创建连接池实例（不再使用单例）
    HttpClientPool pool;
    pool.Init(io_context, config);

    // 测试获取的客户端都是有效的
    auto client1 = pool.Acquire();
    if (client1 && client1->IsValid()) {
        std::cout << "Client 1 is valid" << std::endl;
    }

    auto client2 = pool.Acquire();
    if (client2 && client2->IsValid()) {
        std::cout << "Client 2 is valid" << std::endl;
    }

    // 释放后再次获取
    if (client1) {
        pool.Release(client1);
    }
    if (client2) {
        pool.Release(client2);
    }

    auto client3 = pool.Acquire();
    if (client3) {
        std::cout << "Client 3 acquired, valid: " << client3->IsValid()
                  << ", request_count: " << client3->GetRequestCount() << std::endl;
        pool.Release(client3);
    }

    pool.Stop();
}

/// @brief 测试 RAII 守卫模式（推荐使用）
void TestRaiiGuard() {
    std::cout << "\n=== Test RAII Guard ===" << std::endl;

    boost::asio::io_context io_context;

    HttpClientPool::Config config;
    config.host = "httpbin.org";
    config.port = 80;
    config.init_size = 2;
    config.max_size = 3;

    // 创建连接池实例（不再使用单例）
    HttpClientPool pool;
    pool.Init(io_context, config);

    // 使用 RAII 守卫，无需手动 Release
    {
        auto guard1 = pool.AcquireGuard();
        if (guard1) {
            std::cout << "Guard 1 acquired, valid: " << guard1->IsValid() << std::endl;
        }

        auto guard2 = pool.AcquireGuard();
        if (guard2) {
            std::cout << "Guard 2 acquired, valid: " << guard2->IsValid() << std::endl;
        }

        // guard 离开作用域时自动释放，无需显式调用 Release
    }

    // 检查统计信息，确认客户端已正确释放
    auto stats = pool.GetStats();
    std::cout << "After guards released: total=" << stats.total
              << ", available=" << stats.available
              << ", active=" << stats.active << std::endl;

    pool.Stop();
}

/// @brief 测试池耗尽行为
void TestPoolExhaustion() {
    std::cout << "\n=== Test Pool Exhaustion ===" << std::endl;

    boost::asio::io_context io_context;

    HttpClientPool::Config config;
    config.host = "httpbin.org";
    config.port = 80;
    config.init_size = 2;
    config.max_size = 2;  // 最大只有 2 个

    // 创建连接池实例（不再使用单例）
    HttpClientPool pool;
    pool.Init(io_context, config);

    // 获取 2 个客户端
    auto client1 = pool.Acquire();
    auto client2 = pool.Acquire();

    // 第 3 个应该返回 nullptr
    auto client3 = pool.Acquire();
    if (!client3) {
        std::cout << "Correctly returned nullptr when pool exhausted" << std::endl;
    }

    // 释放一个后再获取
    if (client1) {
        pool.Release(client1);
    }

    auto client4 = pool.Acquire();
    if (client4) {
        std::cout << "Successfully acquired client after release" << std::endl;
        pool.Release(client4);
    }

    if (client2) {
        pool.Release(client2);
    }

    pool.Stop();
}

int main() {
    LogManager& log_manager = LogManager::getInstance();
    log_manager.Init();
    std::cout << "LogManager initialized" << std::endl;

    // 基础功能测试
    TestBasicUsage();
    
    // 统计信息测试
    TestPoolStats();
    
    // 并发访问测试
    TestConcurrentAccess();
    
    // 最大请求数测试
    TestMaxRequests();
    
    // 客户端有效性测试
    TestClientValidity();
    
    // RAII 守卫测试（新增，推荐使用）
    TestRaiiGuard();
    
    // 池耗尽测试
    TestPoolExhaustion();

    std::cout << "\n=== All tests completed! ===" << std::endl;
    return 0;
}
