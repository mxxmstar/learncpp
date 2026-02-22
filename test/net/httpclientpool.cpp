#include "net/httpclientpool.h"
#include "log/logmanager.h"
#include <boost/json.hpp>
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>

void TestBasicUsage() {
    std::cout << "\n=== Test Basic Usage ===" << std::endl;

    boost::asio::io_context io_context;

    HttpClientPool::Config config;
    config.host = "httpbin.org";
    config.port = 80;
    config.init_size = 2;
    config.max_size = 5;

    auto& pool = HttpClientPool::GetInstance();
    pool.Init(io_context, config);

    auto client = pool.Acquire();
    if (!client) {
        std::cout << "Failed to acquire client" << std::endl;
        return;
    }

    std::cout << "Client acquired, valid: " << client->IsValid() << std::endl;

    boost::json::object req_obj;
    req_obj["test"] = "hello";

    client->PostJson("/post", req_obj, [](bool success, const boost::json::object& rsp) {
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

    auto& pool = HttpClientPool::GetInstance();
    pool.Init(io_context, config);

    auto stats = pool.GetStats();
    std::cout << "Initial stats: total=" << stats.total
              << ", available=" << stats.available
              << ", active=" << stats.active << std::endl;

    std::vector<std::shared_ptr<PooledHttpClient>> clients;
    for (int i = 0; i < 4; ++i) {
        auto client = pool.Acquire();
        if (client) {
            clients.push_back(client);
            std::cout << "Acquired client " << i << ", request_count: "
                      << client->GetRequestCount() << std::endl;
        }
    }

    stats = pool.GetStats();
    std::cout << "After acquire 4: total=" << stats.total
              << ", available=" << stats.available
              << ", active=" << stats.active << std::endl;

    clients.clear();

    stats = pool.GetStats();
    std::cout << "After release: total=" << stats.total
              << ", available=" << stats.available
              << ", active=" << stats.active << std::endl;

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

    auto& pool = HttpClientPool::GetInstance();
    pool.Init(io_context, config);

    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    std::atomic<int> fail_count{0};

    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&pool, &success_count, &fail_count, i]() {
            auto client = pool.Acquire();
            if (!client) {
                std::cout << "Thread " << i << " failed to acquire client" << std::endl;
                fail_count++;
                return;
            }

            boost::json::object req_obj;
            req_obj["thread_id"] = i;

            client->PostJson("/post", req_obj, [&success_count, &fail_count, i](bool success, const boost::json::object& rsp) {
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
              << ", created=" << stats.created
              << ", destroyed=" << stats.destroyed << std::endl;
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

    auto& pool = HttpClientPool::GetInstance();
    pool.Init(io_context, config);

    for (int i = 0; i < 5; ++i) {
        auto stats = pool.GetStats();
        std::cout << "Request " << i << ": total=" << stats.total
                  << ", created=" << stats.created
                  << ", destroyed=" << stats.destroyed << std::endl;

        auto client = pool.Acquire();
        if (client) {
            std::cout << "Client request_count: " << client->GetRequestCount() << std::endl;
        }
    }

    auto stats = pool.GetStats();
    std::cout << "Final: created=" << stats.created
              << ", destroyed=" << stats.destroyed << std::endl;

    pool.Stop();
}

int main() {
    LogManager& log_manager = LogManager::getInstance();
    log_manager.Init();
    std::cout << "LogManager initialized" << std::endl;

    TestBasicUsage();
    TestPoolStats();
    TestConcurrentAccess();
    TestMaxRequests();

    std::cout << "\nAll tests completed!" << std::endl;
    return 0;
}
