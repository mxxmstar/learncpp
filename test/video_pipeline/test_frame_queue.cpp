#include <iostream>
#include <thread>
#include <chrono>
#include <cassert>
#include "video_pipeline/frame_queue.h"
#include "video_pipeline/frame_data.h"

/// @brief 测试基本功能
void test_basic_frame_queue() {
    std::cout << "=== Test: Basic Frame Queue ===" << std::endl;
    
    FrameQueue<int> queue(16);
    
    // 测试 push
    for (int i = 0; i < 10; ++i) {
        bool success = queue.push(i);
        assert(success);
        std::cout << "Push " << i << ": OK" << std::endl;
    }
    
    // 验证队列大小
    std::cout << "Queue size: " << queue.size() << std::endl;
    assert(queue.size() == 10);
    
    // 测试 pop
    for (int i = 0; i < 10; ++i) {
        auto opt = queue.pop(std::chrono::milliseconds(100));
        assert(opt.has_value());
        assert(opt.value() == i);
        std::cout << "Pop " << i << ": " << opt.value() << std::endl;
    }
    
    // 验证队列为空
    assert(queue.empty());
    assert(queue.totalPushed() == 10);
    assert(queue.totalPopped() == 10);
    
    std::cout << "Basic frame queue test passed!\n" << std::endl;
}

/// @brief 测试超时行为
void test_timeout() {
    std::cout << "=== Test: Timeout Behavior ===" << std::endl;
    
    FrameQueue<int> queue(8);
    
    // 从空队列弹出（应该超时）
    auto start = std::chrono::steady_clock::now();
    auto opt = queue.pop(std::chrono::milliseconds(500));
    auto end = std::chrono::steady_clock::now();
    
    assert(!opt.has_value());
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "Timeout duration: " << duration << "ms (expected ~500ms)" << std::endl;
    assert(duration >= 450 && duration <= 550);  // 允许一定误差
    
    std::cout << "Timeout test passed!\n" << std::endl;
}

/// @brief 测试关闭行为
void test_shutdown() {
    std::cout << "=== Test: Shutdown Behavior ===" << std::endl;
    
    FrameQueue<int> queue(8);
    
    // 推入一些数据
    queue.push(1);
    queue.push(2);
    queue.push(3);
    
    // 关闭队列
    queue.shutdown();
    assert(queue.isShutdown());
    
    // 尝试推入应该失败
    bool success = queue.push(4);
    assert(!success);
    
    // 仍然可以弹出已有的数据
    auto opt1 = queue.try_pop();
    assert(opt1.has_value() && opt1.value() == 1);
    
    auto opt2 = queue.try_pop();
    assert(opt2.has_value() && opt2.value() == 2);
    
    auto opt3 = queue.try_pop();
    assert(opt3.has_value() && opt3.value() == 3);
    
    // 队列已空，返回 nullopt
    auto opt4 = queue.try_pop();
    assert(!opt4.has_value());
    
    std::cout << "Shutdown test passed!\n" << std::endl;
}

/// @brief 测试生产者 - 消费者模式
void test_producer_consumer() {
    std::cout << "=== Test: Producer-Consumer Pattern ===" << std::endl;
    
    constexpr int QUEUE_SIZE = 64;
    constexpr int ITEMS_TO_PRODUCE = 1000;
    
    FrameQueue<int> queue(QUEUE_SIZE);
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
        std::cout << "Producer finished (" << ITEMS_TO_PRODUCE << " items)" << std::endl;
    });
    
    // 消费者线程
    std::thread consumer([&]() {
        int expected = 0;
        while (expected < ITEMS_TO_PRODUCE) {
            auto opt = queue.pop(std::chrono::milliseconds(100));
            if (opt.has_value()) {
                assert(opt.value() == expected);  // 验证顺序
                expected++;
                consumed_count++;
            }
        }
        std::cout << "Consumer finished, consumed " << consumed_count << " items" << std::endl;
    });
    
    producer.join();
    consumer.join();
    
    assert(consumed_count == ITEMS_TO_PRODUCE);
    assert(queue.totalPushed() == ITEMS_TO_PRODUCE);
    assert(queue.totalPopped() == ITEMS_TO_PRODUCE);
    
    std::cout << "Producer-Consumer test passed!\n" << std::endl;
}

/// @brief 测试 FrameData 传输
void test_framedata_transfer() {
    std::cout << "=== Test: FrameData Transfer ===" << std::endl;
    
    FrameQueue<FrameData> queue(16);
    
    // 创建模拟帧数据
    for (int i = 0; i < 5; ++i) {
        cv::Mat frame(480, 640, CV_8UC3, cv::Scalar(i * 50, 0, 0));
        FrameData data(i, i * 33000, std::move(frame));
        
        bool success = queue.push(std::move(data));
        assert(success);
        std::cout << "Pushed frame " << i << " (" 
                  << data.width << "x" << data.height << ")" << std::endl;
    }
    
    // 消费帧数据
    for (int i = 0; i < 5; ++i) {
        auto opt = queue.pop(std::chrono::milliseconds(100));
        assert(opt.has_value());
        
        const auto& frame_data = opt.value();
        assert(frame_data.channel_id == i);
        assert(frame_data.timestamp_us == i * 33000);
        assert(!frame_data.frame.empty());
        assert(frame_data.width == 640);
        assert(frame_data.height == 480);
        
        std::cout << "Popped frame " << frame_data.channel_id 
                  << " @ " << frame_data.timestamp_us << "us" << std::endl;
    }
    
    std::cout << "FrameData transfer test passed!\n" << std::endl;
}

/// @brief 性能测试
void test_performance() {
    std::cout << "=== Test: Performance Benchmark ===" << std::endl;
    
    constexpr int TEST_ITEMS = 100000;
    constexpr int QUEUE_CAPACITY = 256;
    
    FrameQueue<int> queue(QUEUE_CAPACITY);
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
            auto opt = queue.pop(std::chrono::milliseconds(10));
            if (opt.has_value()) {
                count++;
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
    
    std::cout << "Performance test completed!\n" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "FrameQueue Tests" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    test_basic_frame_queue();
    test_timeout();
    test_shutdown();
    test_producer_consumer();
    test_framedata_transfer();
    test_performance();
    
    std::cout << "========================================" << std::endl;
    std::cout << "All tests passed!" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}
