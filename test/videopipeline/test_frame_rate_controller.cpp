/**
 * FrameRateController 鍗曞厓娴嬭瘯
 * 
 * 娴嬭瘯鐩爣锛?
 * 1. 楠岃瘉甯х巼鎺у埗閫昏緫姝ｇ‘
 * 2. 楠岃瘉缁熻淇℃伅鍑嗙‘
 * 3. 楠岃瘉绾跨▼瀹夊叏
 */

#include "frame_rate_controller.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <cassert>
#include <vector>
#include <cmath>

using namespace video_pipeline;

// ==================== 杈呭姪鍑芥暟 ====================

void PrintTestHeader(const std::string& test_name) {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "Test: " << test_name << std::endl;
    std::cout << std::string(60, '=') << std::endl;
}

void PrintTestResult(bool passed, const std::string& message) {
    if (passed) {
        std::cout << "鉁?PASSED: " << message << std::endl;
    } else {
        std::cout << "鉁?FAILED: " << message << std::endl;
    }
}

// ==================== 娴嬭瘯鐢ㄤ緥 ====================

/**
 * 娴嬭瘯 1: 鍩烘湰甯х巼鎺у埗
 */
void TestBasicFrameRateControl() {
    PrintTestHeader("Basic Frame Rate Control");
    
    // 鍒涘缓鎺у埗鍣紝鐩爣 10 FPS
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
        
        // 妯℃嫙 5ms 闂撮殧锛堟瘮鐩爣 100ms 蹇級
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
    
    // 楠岃瘉锛氬彂閫佺殑甯ф暟搴旇鍦ㄥ悎鐞嗚寖鍥村唴锛堣€冭檻璇樊锛?
    int expected_sent = duration_ms / 100; // 10 FPS = 100ms 闂撮殧
    bool passed = (sent_count >= expected_sent - 2 && sent_count <= expected_sent + 2);
    PrintTestResult(passed, "Frame rate control within expected range");
}

/**
 * 娴嬭瘯 2: 鏃犻檺鍒跺抚鐜囷紙target_fps = 0锛?
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
 * 娴嬭瘯 3: 鍔ㄦ€佷慨鏀圭洰鏍囧抚鐜?
 */
void TestDynamicFpsChange() {
    PrintTestHeader("Dynamic FPS Change");
    
    FrameRateController controller(10);
    
    // 鍏堜互 10 FPS 杩愯
    int sent_at_10fps = 0;
    for (int i = 0; i < 50; ++i) {
        if (controller.shouldSendFrame()) {
            controller.recordFrameSent();
            sent_at_10fps++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    
    std::cout << "At 10 FPS: sent " << sent_at_10fps << " frames" << std::endl;
    
    // 鏀逛负 20 FPS
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
    
    // 楠岃瘉锛?0 FPS 搴旇鍙戦€佹洿澶氬抚
    bool passed = (sent_at_20fps > sent_at_10fps);
    PrintTestResult(passed, "Higher FPS sends more frames");
}

/**
 * 娴嬭瘯 4: 缁熻淇℃伅鍑嗙‘鎬?
 */
void TestStatisticsAccuracy() {
    PrintTestHeader("Statistics Accuracy");
    
    FrameRateController controller(10);
    
    // 鍙戦€?50 甯э紝璺宠繃 50 甯?
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
 * 娴嬭瘯 5: 閲嶇疆缁熻淇℃伅
 */
void TestResetStats() {
    PrintTestHeader("Reset Statistics");
    
    FrameRateController controller(10);
    
    // 鍏堣褰曚竴浜涙暟鎹?
    for (int i = 0; i < 20; ++i) {
        controller.shouldSendFrame();
        controller.recordFrameSent();
    }
    
    std::cout << "Before reset: " << controller.getStatsString() << std::endl;
    
    // 閲嶇疆
    controller.resetStats();
    
    std::cout << "After reset: " << controller.getStatsString() << std::endl;
    
    bool passed = (controller.getTotalSent() == 0 && 
                   controller.getTotalSkipped() == 0);
    PrintTestResult(passed, "Statistics reset successfully");
}

/**
 * 娴嬭瘯 6: 澶氱嚎绋嬪畨鍏ㄦ€?
 */
void TestThreadSafety() {
    PrintTestHeader("Thread Safety");
    
    FrameRateController controller(30);
    
    const int num_threads = 4;
    const int frames_per_thread = 100;
    std::vector<std::thread> threads;
    
    auto start_time = std::chrono::steady_clock::now();
    
    // 鍚姩澶氫釜绾跨▼鍚屾椂浣跨敤鎺у埗鍣?
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
    
    // 绛夊緟鎵€鏈夌嚎绋嬪畬鎴?
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
 * 娴嬭瘯 7: 瀹為檯 FPS 璁＄畻
 */
void TestActualFpsCalculation() {
    PrintTestHeader("Actual FPS Calculation");
    
    FrameRateController controller(10);
    
    // 妯℃嫙鍙戦€?20 甯э紝姣忓抚闂撮殧 100ms
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
    
    // 楠岃瘉锛氬疄闄?FPS 搴旇鎺ヨ繎 10锛堝厑璁?卤2 鐨勮宸級
    bool passed = (actual_fps >= 8.0 && actual_fps <= 12.0);
    PrintTestResult(passed, "Actual FPS calculation is accurate");
}

// ==================== 涓诲嚱鏁?====================

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
        std::cerr << "\n鉁?Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}

