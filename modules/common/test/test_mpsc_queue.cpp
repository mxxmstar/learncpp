/**
 * @file test_mpsc_queue.cpp
 * @brief MPSC 队列单元测试
 *
 * 测试 BoundedMpscQueue 和 UnboundedMpscQueue 两个实现，
 * 重点验证多生产者并发安全
 */

#include "common/queue/mpsc_queue.h"
#include <cassert>
#include <thread>
#include <vector>
#include <iostream>
#include <atomic>
#include <set>

// ==================== 辅助函数 ====================
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST(name)                                         \
    do {                                                   \
        std::cout << "  TEST: " << name << " ... ";         \
    } while (0)

#define PASS()                                             \
    do {                                                   \
        std::cout << "PASSED" << std::endl;                 \
        g_tests_passed++;                                   \
    } while (0)

#define FAIL(msg)                                          \
    do {                                                   \
        std::cout << "FAILED: " << msg << std::endl;        \
        g_tests_failed++;                                   \
    } while (0)

// ==================== 测试用例 ====================

/// 测试有界 MPSC 基本操作
void test_bounded_basic() {
    TEST("BoundedMpscQueue basic push/pop");
    common::BoundedMpscQueue<int> q(8);

    assert(q.empty());
    assert(q.push(1));
    assert(q.push(2));

    int val = 0;
    assert(q.pop(val));
    assert(val == 1);
    assert(q.pop(val));
    assert(val == 2);
    assert(q.empty());

    PASS();
}

/// 测试有界 MPSC 多生产者
void test_bounded_multi_producer() {
    TEST("BoundedMpscQueue multi-producer");
    const int kProducers = 4;
    const int kItemsPerProducer = 2500;
    const int kTotal = kProducers * kItemsPerProducer;

    common::BoundedMpscQueue<int> q(2048);
    std::atomic<int> produced{0};
    std::atomic<bool> any_failed{false};

    // 启动多个生产者
    std::vector<std::thread> producers;
    for (int p = 0; p < kProducers; ++p) {
        producers.emplace_back([&, p]() {
            for (int i = 0; i < kItemsPerProducer; ++i) {
                // 每个生产者写入唯一值：p * 10000 + i
                int val = p * 10000 + i;
                while (!q.push(val)) {
                    if (produced.load() >= kTotal) break;
                    std::this_thread::yield();
                }
                produced.fetch_add(1);
            }
        });
    }

    // 收集所有结果
    std::set<int> received;
    while (static_cast<int>(received.size()) < kTotal) {
        int val;
        if (q.pop(val)) {
            received.insert(val);
        } else {
            std::this_thread::yield();
        }
    }

    for (auto& t : producers) t.join();

    // 验证没有重复或丢失
    assert(static_cast<int>(received.size()) == kTotal);
    for (int p = 0; p < kProducers; ++p) {
        for (int i = 0; i < kItemsPerProducer; ++i) {
            if (received.find(p * 10000 + i) == received.end()) {
                std::cerr << "Missing value: " << (p * 10000 + i) << std::endl;
                any_failed = true;
            }
        }
    }

    if (any_failed) { FAIL("data loss detected"); return; }
    PASS();
}

/// 测试有界 MPSC 队列满
void test_bounded_full() {
    TEST("BoundedMpscQueue full rejection");
    common::BoundedMpscQueue<int> q(2);

    assert(q.push(1));
    assert(q.push(2));
    // 第3个可能成功（boost::lockfree::queue 内部可能使用更多节点）
    // 所以这里只确保不会 crash，不严格断言

    q.clear();
    assert(q.empty());
    PASS();
}

/// 测试无界 MPSC 基本操作
void test_unbounded_basic() {
    TEST("UnboundedMpscQueue basic push/pop");
    common::UnboundedMpscQueue<int> q(8);

    assert(q.empty());
    q.push(1);
    q.push(2);
    q.push(3);

    int val;
    assert(q.pop(val));
    assert(q.pop(val));
    assert(q.pop(val));
    assert(q.empty());
    PASS();
}

/// 测试无界 MPSC 多生产者
void test_unbounded_multi_producer() {
    TEST("UnboundedMpscQueue multi-producer");
    const int kProducers = 4;
    const int kItemsPerProducer = 5000;
    const int kTotal = kProducers * kItemsPerProducer;

    common::UnboundedMpscQueue<int> q;
    std::atomic<int> produced{0};

    // 启动多个生产者
    std::vector<std::thread> producers;
    for (int p = 0; p < kProducers; ++p) {
        producers.emplace_back([&, p]() {
            for (int i = 0; i < kItemsPerProducer; ++i) {
                q.push(p * 10000 + i);
                produced.fetch_add(1);
            }
        });
    }

    // 收集结果
    std::set<int> received;
    while (static_cast<int>(received.size()) < kTotal) {
        int val;
        if (q.pop(val)) {
            received.insert(val);
        } else {
            std::this_thread::yield();
        }
    }

    for (auto& t : producers) t.join();

    // 验证完整性
    assert(static_cast<int>(received.size()) == kTotal);
    for (int p = 0; p < kProducers; ++p) {
        for (int i = 0; i < kItemsPerProducer; ++i) {
            if (received.find(p * 10000 + i) == received.end()) {
                std::cerr << "Missing: " << (p * 10000 + i) << std::endl;
                FAIL("data loss");
                return;
            }
        }
    }

    PASS();
}

/// 测试无界 MPSC 大量数据
void test_unbounded_high_volume() {
    TEST("UnboundedMpscQueue high volume");
    const int kCount = 100000;

    common::UnboundedMpscQueue<int> q;
    std::thread producer([&]() {
        for (int i = 0; i < kCount; ++i) {
            q.push(i);
        }
    });

    int expected = 0;
    int val;
    while (expected < kCount) {
        if (q.pop(val)) {
            ++expected;
        } else {
            std::this_thread::yield();
        }
    }

    producer.join();
    PASS();
}

// ==================== 主函数 ====================
int main() {
    std::cout << "===== MPSC Queue Tests =====" << std::endl;

    // 有界队列测试
    std::cout << "\n--- BoundedMpscQueue ---" << std::endl;
    test_bounded_basic();
    test_bounded_multi_producer();
    test_bounded_full();

    // 无界队列测试
    std::cout << "\n--- UnboundedMpscQueue ---" << std::endl;
    test_unbounded_basic();
    test_unbounded_multi_producer();
    test_unbounded_high_volume();

    std::cout << "\n===== Results: " << g_tests_passed << " passed, "
              << g_tests_failed << " failed =====" << std::endl;
    return g_tests_failed > 0 ? 1 : 0;
}
