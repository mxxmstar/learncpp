/**
 * @file test_spsc_queue.cpp
 * @brief SPSC 队列单元测试
 *
 * 测试 BoundedSpscQueue 和 UnboundedSpscQueue 两个实现，
 * 包括：基本操作、边界条件、多线程并发
 */

#include "common/queue/spsc_queue.h"
#include <cassert>
#include <thread>
#include <vector>
#include <iostream>
#include <atomic>

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

/// 测试有界队列基本操作
void test_bounded_basic() {
    TEST("BoundedSpscQueue basic push/pop");
    common::BoundedSpscQueue<int> q(4);

    assert(q.empty());
    assert(q.capacity() == 4);
    assert(q.available() == 4);

    assert(q.push(1));
    assert(q.push(2));
    assert(q.size() == 2);
    assert(q.available() == 2);

    int val = 0;
    assert(q.pop(val));
    assert(val == 1);
    assert(q.pop(val));
    assert(val == 2);
    assert(q.empty());

    PASS();
}

/// 测试有界队列满时 push 失败
void test_bounded_full() {
    TEST("BoundedSpscQueue full rejection");
    common::BoundedSpscQueue<int> q(2);

    assert(q.push(1));
    assert(q.push(2));
    assert(!q.push(3));  // 队列满，应失败

    PASS();
}

/// 测试有界队列清空
void test_bounded_clear() {
    TEST("BoundedSpscQueue clear");
    common::BoundedSpscQueue<int> q(4);
    q.push(1);
    q.push(2);
    q.push(3);
    q.clear();
    assert(q.empty());
    int val;
    assert(!q.pop(val));
    PASS();
}

/// 测试有界队列多线程 SPSC
void test_bounded_concurrent() {
    TEST("BoundedSpscQueue concurrent SPSC");
    const int kCount = 10000;
    common::BoundedSpscQueue<int> q(1024);
    std::atomic<bool> done{false};

    std::thread producer([&]() {
        for (int i = 0; i < kCount; ++i) {
            while (!q.push(i)) {
                std::this_thread::yield();
            }
        }
        done = true;
    });

    std::thread consumer([&]() {
        int expected = 0;
        int val;
        while (expected < kCount) {
            if (q.pop(val)) {
                if (val != expected) {
                    std::cerr << "out of order: expected " << expected
                              << " got " << val << std::endl;
                    return;
                }
                ++expected;
            } else if (done) {
                break;
            } else {
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();
    PASS();
}

/// 测试无界队列基本操作
void test_unbounded_basic() {
    TEST("UnboundedSpscQueue basic push/pop");
    common::UnboundedSpscQueue<int> q;

    assert(q.empty());
    q.push(1);
    q.push(2);
    q.push(3);
    assert(q.size() == 3);

    int val;
    assert(q.pop(val));
    assert(val == 1);
    assert(q.pop(val));
    assert(val == 2);
    assert(q.size() == 1);

    assert(q.try_pop(val));
    assert(val == 3);
    assert(q.empty());
    PASS();
}

/// 测试无界队列永不拒满
void test_unbounded_unlimited() {
    TEST("UnboundedSpscQueue unlimited growth");
    common::UnboundedSpscQueue<int> q;
    const int kCount = 100000;

    for (int i = 0; i < kCount; ++i) {
        q.push(i);
    }
    assert(q.size() == static_cast<size_t>(kCount));
    PASS();
}

/// 测试无界队列超时弹出
void test_unbounded_timeout() {
    TEST("UnboundedSpscQueue pop_for timeout");
    common::UnboundedSpscQueue<int> q;
    int val = 0;

    // 空队列超时应返回 false
    auto start = std::chrono::steady_clock::now();
    bool result = q.pop_for(val, std::chrono::milliseconds(50));
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);

    assert(!result);                     // 应该超时
    assert(elapsed.count() >= 40);        // 确实等待了

    // push 后应立即能 pop
    q.push(42);
    assert(q.pop_for(val, std::chrono::milliseconds(100)));
    assert(val == 42);
    PASS();
}

/// 测试无界队列关闭
void test_unbounded_close() {
    TEST("UnboundedSpscQueue close");
    common::UnboundedSpscQueue<int> q;
    q.push(1);
    q.push(2);
    q.close();

    int val;
    assert(q.pop(val));   // 关闭后仍可消费已有数据
    assert(q.pop(val));
    assert(!q.pop(val));  // 消费完毕返回 false
    PASS();
}

/// 测试无界队列多线程
void test_unbounded_concurrent() {
    TEST("UnboundedSpscQueue concurrent");
    const int kCount = 10000;
    common::UnboundedSpscQueue<int> q;

    std::thread producer([&]() {
        for (int i = 0; i < kCount; ++i) {
            q.push(i);
        }
        q.close();
    });

    std::thread consumer([&]() {
        int expected = 0;
        int val;
        while (q.pop(val)) {
            if (val != expected) {
                std::cerr << "out of order at " << expected << std::endl;
                return;
            }
            ++expected;
        }
    });

    producer.join();
    consumer.join();
    PASS();
}

// ==================== 主函数 ====================
int main() {
    std::cout << "===== SPSC Queue Tests =====" << std::endl;

    // 有界队列测试
    std::cout << "\n--- BoundedSpscQueue ---" << std::endl;
    test_bounded_basic();
    test_bounded_full();
    test_bounded_clear();
    test_bounded_concurrent();

    // 无界队列测试
    std::cout << "\n--- UnboundedSpscQueue ---" << std::endl;
    test_unbounded_basic();
    test_unbounded_unlimited();
    test_unbounded_timeout();
    test_unbounded_close();
    test_unbounded_concurrent();

    // 汇总
    std::cout << "\n===== Results: " << g_tests_passed << " passed, "
              << g_tests_failed << " failed =====" << std::endl;
    return g_tests_failed > 0 ? 1 : 0;
}
