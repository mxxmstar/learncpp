/**
 * FrameRateController 单元测试
 * 
 * 测试目标：
 * 1. 验证帧率控制逻辑正确
 * 2. 验证统计信息准确
 * 3. 验证线程安全
 */

#include "video_pipeline/frame_rate_controller.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <cassert>
#include <vector>
#include <cmath>

using namespace video_pipeline;

// ==================== 辅助函数 ====================

void PrintTestHeader(const std::string& test_name) {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "Test: " << test_name << std::endl;
    std::cout << std::string(60, '=') << std::endl;
}

void PrintTestResult(bool passed, const std::string& message) {
    if (passed) {
        std::cout << "✓ PASSED: " << message << std::endl;
    } else {
        std::cout << "✗ FAILED: " << message << std::endl;
    }
}

// ==================== 测试用例 ====================

/**
 * 测试 1: 基本帧率控制
 */
void TestBasicFrameRateControl() {
    PrintTestHeader("Basic Frame Rate Control");
    
    // 创建控制器，目标 10 FPS
    FrameRateController controller(10);
    
    int sent_count = 0;
    int skipped_count = 0;
    int total_frames = 100;
    
    auto start_time = std::chrono::steady_clock::now();
    
    for (int i = 0; i < total_frames; ++i) {
        if (controller.shouldSendFrame()) {
            controller.recordFrameSent();
            sent_count++;
        } else {
            controller.recordFrameSkipped();
            skipped_count++;
        }
        
        // 模拟 5ms 间隔（比目标 100ms 快）
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();
    
    std::cout << "Total frames: " << total_frames << std::endl;
    std::cout << "Sent: " << sent_count << std::endl;
    std::cout << "Skipped: " << skipped_count << std::endl;
    std::cout << "Duration: " << duration_ms << "ms" << std::endl;
    std::cout << controller.getStatsString() << std::endl;
    
    // 验证：发送的帧数应该在合理范围内（考虑误差）
    int expected_sent = duration_ms / 100; // 10 FPS = 100ms 间隔
    bool passed = (sent_count >= expected_sent - 2 && sent_count <= expected_sent + 2);
    PrintTestResult(passed, "Frame rate control within expected range");
}

/**
 * 测试 2: 无限制帧率（target_fps = 0）
 */
void TestUnlimitedFrameRate() {
    PrintTestHeader("Unlimited Frame Rate (target_fps = 0)");
    
    FrameRateController controller(0);
    
    int sent_count = 0;
    int total_frames = 100;
    
    for (int i = 0; i < total_frames; ++i) {
        if (controller.shouldSendFrame()) {
            controller.recordFrameSent();
            sent_count++;
        }
    }
    
    std::cout << "Total frames: " << total_frames << std::endl;
    std::cout << "Sent: " << sent_count << std::endl;
    std::cout << controller.getStatsString() << std::endl;
    
    bool passed = (sent_count == total_frames);
    PrintTestResult(passed, "All frames sent when unlimited");
}

/**
 * 测试 3: 动态修改目标帧率
 */
void TestDynamicFpsChange() {
    PrintTestHeader("Dynamic FPS Change");
    
    FrameRateController controller(10);
    
    // 先以 10 FPS 运行
    int sent_at_10fps = 0;
    for (int i = 0; i < 50; ++i) {
        if (controller.shouldSendFrame()) {
            controller.recordFrameSent();
            sent_at_10fps++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    
    std::cout << "At 10 FPS: sent " << sent_at_10fps << " frames" << std::endl;
    
    // 改为 20 FPS
    controller.setTargetFps(20);
    
    int sent_at_20fps = 0;
    for (int i = 0; i < 50; ++i) {
        if (controller.shouldSendFrame()) {
            controller.recordFrameSent();
            sent_at_20fps++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    
    std::cout << "At 20 FPS: sent " << sent_at_20fps << " frames" << std::endl;
    std::cout << controller.getStatsString() << std::endl;
    
    // 验证：20 FPS 应该发送更多帧
    bool passed = (sent_at_20fps > sent_at_10fps);
    PrintTestResult(passed, "Higher FPS sends more frames");
}

/**
 * 测试 4: 统计信息准确性
 */
void TestStatisticsAccuracy() {
    PrintTestHeader("Statistics Accuracy");
    
    FrameRateController controller(10);
    
    // 发送 50 帧，跳过 50 帧
    for (int i = 0; i < 100; ++i) {
        if (i % 2 == 0) {
            controller.shouldSendFrame();
            controller.recordFrameSent();
        } else {
            controller.recordFrameSkipped();
        }
    }
    
    uint64_t sent = controller.getTotalSent();
    uint64_t skipped = controller.getTotalSkipped();
    double skip_rate = controller.getSkipRate();
    
    std::cout << "Sent: " << sent << std::endl;
    std::cout << "Skipped: " << skipped << std::endl;
    std::cout << "Skip rate: " << skip_rate << "%" << std::endl;
    
    bool passed = (sent == 50 && skipped == 50 && std::abs(skip_rate - 50.0) < 0.1);
    PrintTestResult(passed, "Statistics are accurate");
}

/**
 * 测试 5: 重置统计信息
 */
void TestResetStats() {
    PrintTestHeader("Reset Statistics");
    
    FrameRateController controller(10);
    
    // 先记录一些数据
    for (int i = 0; i < 20; ++i) {
        controller.shouldSendFrame();
        controller.recordFrameSent();
    }
    
    std::cout << "Before reset: " << controller.getStatsString() << std::endl;
    
    // 重置
    controller.resetStats();
    
    std::cout << "After reset: " << controller.getStatsString() << std::endl;
    
    bool passed = (controller.getTotalSent() == 0 && 
                   controller.getTotalSkipped() == 0);
    PrintTestResult(passed, "Statistics reset successfully");
}

/**
 * 测试 6: 多线程安全性
 */
void TestThreadSafety() {
    PrintTestHeader("Thread Safety");
    
    FrameRateController controller(30);
    
    const int num_threads = 4;
    const int frames_per_thread = 100;
    std::vector<std::thread> threads;
    
    auto start_time = std::chrono::steady_clock::now();
    
    // 启动多个线程同时使用控制器
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&controller, frames_per_thread]() {
            for (int i = 0; i < frames_per_thread; ++i) {
                if (controller.shouldSendFrame()) {
                    controller.recordFrameSent();
                } else {
                    controller.recordFrameSkipped();
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();
    
    uint64_t total_sent = controller.getTotalSent();
    uint64_t total_skipped = controller.getTotalSkipped();
    uint64_t total = total_sent + total_skipped;
    int expected_total = num_threads * frames_per_thread;
    
    std::cout << "Threads: " << num_threads << std::endl;
    std::cout << "Frames per thread: " << frames_per_thread << std::endl;
    std::cout << "Total processed: " << total << std::endl;
    std::cout << "Expected: " << expected_total << std::endl;
    std::cout << "Sent: " << total_sent << std::endl;
    std::cout << "Skipped: " << total_skipped << std::endl;
    std::cout << "Duration: " << duration_ms << "ms" << std::endl;
    std::cout << controller.getStatsString() << std::endl;
    
    bool passed = (total == static_cast<uint64_t>(expected_total));
    PrintTestResult(passed, "Thread-safe operation (no data loss)");
}

/**
 * 测试 7: 实际 FPS 计算
 */
void TestActualFpsCalculation() {
    PrintTestHeader("Actual FPS Calculation");
    
    FrameRateController controller(10);
    
    // 模拟发送 20 帧，每帧间隔 100ms
    for (int i = 0; i < 20; ++i) {
        if (controller.shouldSendFrame()) {
            controller.recordFrameSent();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    double actual_fps = controller.getActualFps();
    
    std::cout << "Target FPS: 10" << std::endl;
    std::cout << "Actual FPS: " << actual_fps << std::endl;
    std::cout << controller.getStatsString() << std::endl;
    
    // 验证：实际 FPS 应该接近 10（允许 ±2 的误差）
    bool passed = (actual_fps >= 8.0 && actual_fps <= 12.0);
    PrintTestResult(passed, "Actual FPS calculation is accurate");
}

// ==================== 主函数 ====================

int main() {
    std::cout << "\n" << std::string(70, '#') << std::endl;
    std::cout << "# FrameRateController Unit Tests" << std::endl;
    std::cout << std::string(70, '#') << std::endl;
    
    try {
        TestBasicFrameRateControl();
        TestUnlimitedFrameRate();
        TestDynamicFpsChange();
        TestStatisticsAccuracy();
        TestResetStats();
        TestThreadSafety();
        TestActualFpsCalculation();
        
        std::cout << "\n" << std::string(70, '#') << std::endl;
        std::cout << "# All tests completed!" << std::endl;
        std::cout << std::string(70, '#') << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
