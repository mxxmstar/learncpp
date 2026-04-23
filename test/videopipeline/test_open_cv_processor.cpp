#include <iostream>
#include <opencv2/opencv.hpp>
#include "format_converter/opencv_format_converter.h"
#include "common/log/logmanager.h"

extern "C" {
#include <libavutil/imgutils.h>
}

/// @brief 鍒涘缓妯℃嫙鐨?YUV420P 甯ф暟鎹?
VideoFrame createMockYUVFrame(int width, int height) {
    VideoFrame frame;
    frame.width = width;
    frame.height = height;
    frame.format = AV_PIX_FMT_YUV420P;  // YUV420P 鏍煎紡
    frame.pts = 0;
    
    // 璁＄畻姣忎釜骞抽潰鐨勫ぇ灏?
    int y_size = width * height;
    int uv_size = y_size / 4;
    
    // 鍒嗛厤 Y 骞抽潰
    frame.data[0] = static_cast<uint8_t*>(av_malloc(y_size));
    frame.linesize[0] = width;
    
    // 鍒嗛厤 U 骞抽潰
    frame.data[1] = static_cast<uint8_t*>(av_malloc(uv_size));
    frame.linesize[1] = width / 2;
    
    // 鍒嗛厤 V 骞抽潰
    frame.data[2] = static_cast<uint8_t*>(av_malloc(uv_size));
    frame.linesize[2] = width / 2;
    
    // 濉厖娴嬭瘯鏁版嵁锛圷: 娓愬彉鐏板害锛孶/V: 鍥哄畾鍊硷級
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            frame.data[0][y * width + x] = static_cast<uint8_t>((x + y) * 255 / (width + height));
        }
    }
    
    memset(frame.data[1], 128, uv_size);  // U = 128 (neutral)
    memset(frame.data[2], 128, uv_size);  // V = 128 (neutral)
    
    return frame;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "OpenCVFrameProcessor Test (YUV -> BGR)" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    try {
        // 鍒濆鍖栨棩蹇?
        LogManager& log_mgr = LogManager::getInstance();
        log_mgr.Init();
        
        // 鍒涘缓 OpenCV 鏍煎紡杞崲鍣?
        format_converter::OpenCVFormatConverter converter;
        
        // 娴嬭瘯 1锛氳浆鎹?YUV420P 鍒?BGR
        std::cout << "\n--- Test 1: YUV420P to BGR Conversion ---" << std::endl;
        auto yuv_frame = createMockYUVFrame(640, 480);
        
        cv::Mat result;
        int64_t pts;
        
        converter.process(std::move(yuv_frame), [&](cv::Mat&& mat, int64_t timestamp) {
            result = std::move(mat);
            pts = timestamp;
        });
        
        if (!result.empty()) {
            std::cout << "Converted image: " << result.cols << "x" 
                      << result.rows << "x" << result.channels() << std::endl;
            std::cout << "PTS: " << pts << "ms" << std::endl;
            cv::imwrite("output_test1_yuv_to_bgr.jpg", result);
            std::cout << "Saved to: output_test1_yuv_to_bgr.jpg" << std::endl;
        } else {
            std::cerr << "Failed to convert frame!" << std::endl;
            return 1;
        }
        
        // 娴嬭瘯 2锛氫笉鍚屽垎杈ㄧ巼
        std::cout << "\n--- Test 2: Different Resolutions ---" << std::endl;
        std::vector<std::pair<int, int>> resolutions = {
            {320, 240},
            {640, 480},
            {1280, 720},
            {1920, 1080}
        };
        
        for (const auto& [w, h] : resolutions) {
            auto frame = createMockYUVFrame(w, h);
            cv::Mat converted;
            
            converter.process(std::move(frame), [&](cv::Mat&& mat, int64_t) {
                converted = std::move(mat);
            });
            
            if (!converted.empty()) {
                std::cout << "  " << w << "x" << h << " -> " 
                          << converted.cols << "x" << converted.rows 
                          << "x" << converted.channels() << " 鉁? << std::endl;
            } else {
                std::cout << "  " << w << "x" << h << " -> Failed 鉁? << std::endl;
            }
        }
        
        // 娴嬭瘯 3锛氱┖甯у鐞?
        std::cout << "\n--- Test 3: Empty Frame Handling ---" << std::endl;
        VideoFrame empty_frame;
        bool callback_called = false;
        
        converter.process(std::move(empty_frame), [&](cv::Mat&&, int64_t) {
            callback_called = true;
        });
        
        std::cout << "Empty frame handled: " << (callback_called ? "Callback called" : "Callback skipped") << std::endl;
        
        // 娴嬭瘯 4锛氭€ц兘娴嬭瘯
        std::cout << "\n--- Test 4: Performance Test ---" << std::endl;
        const int iterations = 100;
        auto start_time = std::chrono::steady_clock::now();
        
        for (int i = 0; i < iterations; ++i) {
            auto frame = createMockYUVFrame(640, 480);
            converter.process(std::move(frame), [](cv::Mat&&, int64_t) {
                // Do nothing
            });
        }
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time).count();
        
        std::cout << "Processed " << iterations << " frames in " << duration << "ms" << std::endl;
        std::cout << "Average: " << (duration / iterations) << "ms per frame" << std::endl;
        std::cout << "FPS: " << (iterations * 1000.0 / duration) << std::endl;
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "All tests completed successfully!" << std::endl;
        std::cout << "========================================" << std::endl;
        
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

