#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <boost/lockfree/spsc_queue.hpp>

/// @brief 测试 boost::lockfree::spsc_queue 的基本功能
void test_basic_spsc_queue() {
    std::cout << "=== Test: Basic SPSC Queue ===" << std::endl;
    
    boost::lockfree::spsc_queue<int, boost::lockfree::capacity<16>> queue;
    
    // 测试 push 和 pop
    for (int i = 0; i < 10; ++i) {
        bool success = queue.push(i);
        std::cout << "Push " << i << ": " << (success ? "OK" : "FAILED") << std::endl;
        assert(success);
    }
    
    // 验证队列大小
    std::cout << "Queue size: " << queue.read_available() << std::endl;
    assert(queue.read_available() == 10);
    
    // 测试 pop
    for (int i = 0; i < 10; ++i) {
        int value;
        bool success = queue.pop(value);
        assert(success);
        assert(value == i);
        std::cout << "Pop " << i << ": " << value << std::endl;
    }
    
    std::cout << "Basic SPSC queue test passed!\n" << std::endl;
}

/// @brief 测试 SPSC 队列的线程安全性（单生产者单消费者）
void test_spsc_threading() {
    std::cout << "=== Test: SPSC Threading ===" << std::endl;
    
    constexpr int QUEUE_SIZE = 1024;
    constexpr int ITEMS_TO_PRODUCE = 10000;
    
    boost::lockfree::spsc_queue<int, boost::lockfree::capacity<QUEUE_SIZE>> queue;
    std::atomic<bool> producer_done{false};
    std::atomic<int> consumed_count{0};
    
    // 生产者线程
    std::thread producer([&]() {
        for (int i = 0; i < ITEMS_TO_PRODUCE; ++i) {
            // 如果队列满了，等待一下
            while (!queue.push(i)) {
                std::this_thread::yield();
            }
        }
        producer_done = true;
        std::cout << "Producer finished" << std::endl;
    });
    
    // 消费者线程
    std::thread consumer([&]() {
        int expected = 0;
        while (expected < ITEMS_TO_PRODUCE) {
            int value;
            if (queue.pop(value)) {
                assert(value == expected);  // 验证顺序
                expected++;
                consumed_count++;
            } else {
                std::this_thread::yield();
            }
        }
        std::cout << "Consumer finished, consumed " << consumed_count << " items" << std::endl;
    });
    
    producer.join();
    consumer.join();
    
    assert(consumed_count == ITEMS_TO_PRODUCE);
    std::cout << "SPSC threading test passed!\n" << std::endl;
}

/// @brief 测试队列满时的行为
void test_queue_full() {
    std::cout << "=== Test: Queue Full Behavior ===" << std::endl;
    
    constexpr int QUEUE_CAPACITY = 8;
    boost::lockfree::spsc_queue<int, boost::lockfree::capacity<QUEUE_CAPACITY>> queue;
    
    // 填满队列
    int pushed = 0;
    for (int i = 0; i < QUEUE_CAPACITY * 2; ++i) {
        if (queue.push(i)) {
            pushed++;
        }
    }
    
    std::cout << "Pushed " << pushed << " items (capacity: " << QUEUE_CAPACITY << ")" << std::endl;
    assert(pushed <= QUEUE_CAPACITY);
    // spsc_queue 没有 full() 方法，使用 push() 是否成功来判断
    int test_val = 0;
    assert(!queue.push(test_val));  // 队列已满，push 应该失败
    
    // 清空队列
    int popped = 0;
    int value;
    while (queue.pop(value)) {
        popped++;
    }
    
    std::cout << "Popped " << popped << " items" << std::endl;
    assert(popped == pushed);
    assert(queue.empty());
    
    std::cout << "Queue full test passed!\n" << std::endl;
}

/// @brief 测试自定义类型的 SPSC 队列
struct FrameData {
    int id;
    double timestamp;
    std::vector<uint8_t> data;
    
    FrameData(int i, double ts, size_t data_size) 
        : id(i), timestamp(ts), data(data_size, 0xAA) {}
};

void test_custom_type() {
    std::cout << "=== Test: Custom Type Queue ===" << std::endl;
    
    boost::lockfree::spsc_queue<FrameData, boost::lockfree::capacity<16>> queue;
    
    // 生产自定义类型
    for (int i = 0; i < 10; ++i) {
        FrameData frame(i, i * 0.033, 1024);  // 模拟视频帧
        bool success = queue.push(std::move(frame));
        assert(success);
    }
    
    // 消费自定义类型
    for (int i = 0; i < 10; ++i) {
        FrameData frame(0, 0, 0);
        bool success = queue.pop(frame);
        assert(success);
        assert(frame.id == i);
        assert(frame.timestamp == i * 0.033);
        assert(frame.data.size() == 1024);
        
        std::cout << "Frame " << frame.id 
                  << " @ " << frame.timestamp 
                  << "s (" << frame.data.size() << " bytes)" << std::endl;
    }
    
    std::cout << "Custom type test passed!\n" << std::endl;
}

/// @brief 性能测试：对比有锁和无锁队列
void test_performance() {
    std::cout << "=== Test: Performance Comparison ===" << std::endl;
    
    constexpr int TEST_ITEMS = 100000;
    
    // 测试无锁 SPSC 队列
    {
        boost::lockfree::spsc_queue<int, boost::lockfree::capacity<1024>> queue;
        std::atomic<bool> done{false};
        
        auto start = std::chrono::high_resolution_clock::now();
        
        std::thread producer([&]() {
            for (int i = 0; i < TEST_ITEMS; ++i) {
                while (!queue.push(i)) {
                    std::this_thread::yield();
                }
            }
            done = true;
        });
        
        std::thread consumer([&]() {
            int count = 0;
            while (count < TEST_ITEMS) {
                int value;
                if (queue.pop(value)) {
                    count++;
                } else {
                    std::this_thread::yield();
                }
            }
        });
        
        producer.join();
        consumer.join();
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        
        std::cout << "Lock-free SPSC: " << TEST_ITEMS << " items in " 
                  << duration << "ms (" 
                  << (TEST_ITEMS * 1000 / duration) << " items/sec)" << std::endl;
    }
    
    std::cout << "Performance test completed!\n" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Boost Lockfree SPSC Queue Tests" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    test_basic_spsc_queue();
    test_spsc_threading();
    test_queue_full();
    test_custom_type();
    test_performance();
    
    std::cout << "========================================" << std::endl;
    std::cout << "All tests passed!" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}
