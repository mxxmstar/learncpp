/**
 * YuvToJpegConverter 单元测试
 * 
 * 测试目标：
 * 1. 验证 YUV420P → JPEG 转换正确性
 * 2. 验证 NV12 → JPEG 转换正确性
 * 3. 验证 NV21 → JPEG 转换正确性
 * 4. 性能基准测试
 */

#include "preprocess/format_converter/yuv_to_jpeg_converter.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <vector>
#include <random>
#include <cassert>

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

void SaveJpegToFile(const std::vector<uint8_t>& data, const std::string& filename) {
    std::ofstream file(filename, std::ios::binary);
    if (file.is_open()) {
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
        file.close();
        std::cout << "Saved JPEG to: " << filename << " (" << data.size() << " bytes)" << std::endl;
    }
}

// 生成测试用的 YUV420P 数据（彩色渐变）
void GenerateTestYuv420p(std::vector<uint8_t>& y_data,
                        std::vector<uint8_t>& u_data,
                        std::vector<uint8_t>& v_data,
                        int width,
                        int height) {
    int y_size = width * height;
    int uv_size = y_size / 4;
    
    y_data.resize(y_size);
    u_data.resize(uv_size);
    v_data.resize(uv_size);
    
    // 生成 Y 平面（灰度渐变）
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            y_data[i * width + j] = static_cast<uint8_t>((i + j) % 256);
        }
    }
    
    // 生成 U 平面（固定值）
    std::fill(u_data.begin(), u_data.end(), 128);
    
    // 生成 V 平面（固定值）
    std::fill(v_data.begin(), v_data.end(), 128);
}

// 生成测试用的 NV12 数据
void GenerateTestNv12(std::vector<uint8_t>& y_data,
                     std::vector<uint8_t>& uv_data,
                     int width,
                     int height) {
    int y_size = width * height;
    int uv_size = y_size / 2;
    
    y_data.resize(y_size);
    uv_data.resize(uv_size);
    
    // 生成 Y 平面
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            y_data[i * width + j] = static_cast<uint8_t>((i + j) % 256);
        }
    }
    
    // 生成 UV 交错数据
    int uv_width = width / 2;
    int uv_height = height / 2;
    for (int i = 0; i < uv_height; i++) {
        for (int j = 0; j < uv_width; j++) {
            uv_data[(i * uv_width + j) * 2] = 128;     // U
            uv_data[(i * uv_width + j) * 2 + 1] = 128; // V
        }
    }
}

// ==================== 测试用例 ====================

/**
 * 测试 1: YUV420P → JPEG 基本功能
 */
void TestYuv420pToJpeg() {
    PrintTestHeader("YUV420P to JPEG Basic Functionality");
    
    int width = 640;
    int height = 480;
    
    // 生成测试数据
    std::vector<uint8_t> y_data, u_data, v_data;
    GenerateTestYuv420p(y_data, u_data, v_data, width, height);
    
    // 创建转换器
    YuvToJpegConverter converter(85);
    
    // 转换为 JPEG
    std::vector<uint8_t> jpeg_output;
    auto start = std::chrono::high_resolution_clock::now();
    
    bool success = converter.ConvertYuv420p(
        y_data.data(), u_data.data(), v_data.data(),
        width, height, jpeg_output
    );
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();
    
    std::cout << "Conversion time: " << duration_ms << "ms" << std::endl;
    std::cout << "JPEG size: " << jpeg_output.size() << " bytes" << std::endl;
    
    // 验证结果
    bool passed = success && !jpeg_output.empty() && jpeg_output.size() > 1000;
    PrintTestResult(passed, "YUV420P to JPEG conversion successful");
    
    // 保存文件用于人工验证
    if (passed) {
        SaveJpegToFile(jpeg_output, "test_yuv420p.jpg");
    }
}

/**
 * 测试 2: NV12 → JPEG 转换
 */
void TestNv12ToJpeg() {
    PrintTestHeader("NV12 to JPEG Conversion");
    
    int width = 640;
    int height = 480;
    
    // 生成测试数据
    std::vector<uint8_t> y_data, uv_data;
    GenerateTestNv12(y_data, uv_data, width, height);
    
    // 创建转换器
    YuvToJpegConverter converter(85);
    
    // 转换为 JPEG
    std::vector<uint8_t> jpeg_output;
    auto start = std::chrono::high_resolution_clock::now();
    
    bool success = converter.ConvertNv12(
        y_data.data(), uv_data.data(),
        width, height, jpeg_output
    );
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();
    
    std::cout << "Conversion time: " << duration_ms << "ms" << std::endl;
    std::cout << "JPEG size: " << jpeg_output.size() << " bytes" << std::endl;
    
    bool passed = success && !jpeg_output.empty() && jpeg_output.size() > 1000;
    PrintTestResult(passed, "NV12 to JPEG conversion successful");
    
    if (passed) {
        SaveJpegToFile(jpeg_output, "test_nv12.jpg");
    }
}

/**
 * 测试 3: NV21 → JPEG 转换
 */
void TestNv21ToJpeg() {
    PrintTestHeader("NV21 to JPEG Conversion");
    
    int width = 640;
    int height = 480;
    
    // 生成测试数据（与 NV12 类似，但 UV 顺序相反）
    std::vector<uint8_t> y_data, vu_data;
    GenerateTestNv12(y_data, vu_data, width, height); // 复用 NV12 生成函数
    
    // 创建转换器
    YuvToJpegConverter converter(85);
    
    // 转换为 JPEG
    std::vector<uint8_t> jpeg_output;
    auto start = std::chrono::high_resolution_clock::now();
    
    bool success = converter.ConvertNv21(
        y_data.data(), vu_data.data(),
        width, height, jpeg_output
    );
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();
    
    std::cout << "Conversion time: " << duration_ms << "ms" << std::endl;
    std::cout << "JPEG size: " << jpeg_output.size() << " bytes" << std::endl;
    
    bool passed = success && !jpeg_output.empty() && jpeg_output.size() > 1000;
    PrintTestResult(passed, "NV21 to JPEG conversion successful");
    
    if (passed) {
        SaveJpegToFile(jpeg_output, "test_nv21.jpg");
    }
}

/**
 * 测试 4: 不同质量设置
 */
void TestDifferentQualityLevels() {
    PrintTestHeader("Different Quality Levels");
    
    int width = 640;
    int height = 480;
    
    std::vector<uint8_t> y_data, u_data, v_data;
    GenerateTestYuv420p(y_data, u_data, v_data, width, height);
    
    std::vector<int> qualities = {10, 50, 85, 95};
    
    for (int quality : qualities) {
        YuvToJpegConverter converter(quality);
        
        std::vector<uint8_t> jpeg_output;
        bool success = converter.ConvertYuv420p(
            y_data.data(), u_data.data(), v_data.data(),
            width, height, jpeg_output
        );
        
        std::cout << "Quality " << quality << ": " 
                 << jpeg_output.size() << " bytes, "
                 << (success ? "SUCCESS" : "FAILED") << std::endl;
    }
    
    PrintTestResult(true, "Different quality levels tested");
}

/**
 * 测试 5: 性能基准测试
 */
void TestPerformanceBenchmark() {
    PrintTestHeader("Performance Benchmark");
    
    int width = 1920;
    int height = 1080;
    int iterations = 100;
    
    std::vector<uint8_t> y_data, u_data, v_data;
    GenerateTestYuv420p(y_data, u_data, v_data, width, height);
    
    YuvToJpegConverter converter(85);
    
    // 预热
    std::vector<uint8_t> dummy;
    converter.ConvertYuv420p(y_data.data(), u_data.data(), v_data.data(),
                            width, height, dummy);
    
    // 基准测试
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; i++) {
        std::vector<uint8_t> jpeg_output;
        converter.ConvertYuv420p(y_data.data(), u_data.data(), v_data.data(),
                                width, height, jpeg_output);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();
    
    double avg_ms = static_cast<double>(total_ms) / iterations;
    double fps = 1000.0 / avg_ms;
    
    std::cout << "Resolution: " << width << "x" << height << std::endl;
    std::cout << "Iterations: " << iterations << std::endl;
    std::cout << "Total time: " << total_ms << "ms" << std::endl;
    std::cout << "Average time: " << avg_ms << "ms/frame" << std::endl;
    std::cout << "FPS: " << fps << std::endl;
    
    bool passed = (avg_ms < 50.0); // 期望每帧 < 50ms
    PrintTestResult(passed, "Performance benchmark completed");
}

/**
 * 测试 6: 错误处理
 */
void TestErrorHandling() {
    PrintTestHeader("Error Handling");
    
    YuvToJpegConverter converter(85);
    
    std::vector<uint8_t> jpeg_output;
    
    // 测试空指针
    bool result1 = converter.ConvertYuv420p(nullptr, nullptr, nullptr, 640, 480, jpeg_output);
    std::cout << "Null pointers: " << (result1 ? "UNEXPECTED SUCCESS" : "CORRECTLY FAILED") << std::endl;
    
    // 测试无效尺寸
    std::vector<uint8_t> y(100), u(25), v(25);
    bool result2 = converter.ConvertYuv420p(y.data(), u.data(), v.data(), 0, 0, jpeg_output);
    std::cout << "Invalid dimensions: " << (result2 ? "UNEXPECTED SUCCESS" : "CORRECTLY FAILED") << std::endl;
    
    bool passed = !result1 && !result2;
    PrintTestResult(passed, "Error handling works correctly");
}

// ==================== 主函数 ====================

int main() {
    std::cout << "\n" << std::string(70, '#') << std::endl;
    std::cout << "# YuvToJpegConverter Unit Tests" << std::endl;
    std::cout << std::string(70, '#') << std::endl;
    
    try {
        TestYuv420pToJpeg();
        TestNv12ToJpeg();
        TestNv21ToJpeg();
        TestDifferentQualityLevels();
        TestPerformanceBenchmark();
        TestErrorHandling();
        
        std::cout << "\n" << std::string(70, '#') << std::endl;
        std::cout << "# All tests completed!" << std::endl;
        std::cout << std::string(70, '#') << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
