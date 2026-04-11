#include <iostream>
#include "preprocess/format_converter/i_format_converter.h"
#include "preprocess/format_converter/yuv_to_bgr_converter.h"
#include "log/logmanager.h"

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "FormatConverter Test" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    try {
        // 鍒濆鍖栨棩蹇?
        LogManager& log_mgr = LogManager::getInstance();
        log_mgr.Init();
        
        // 鍒涘缓 YUV 鍒?BGR 杞崲鍣?
        YuvToBgrConverter converter;
        
        // 鍒涘缓娴嬭瘯鏁版嵁锛圷UV420P 鏍煎紡锛?
        int width = 640;
        int height = 480;
        int y_size = width * height;
        int uv_size = y_size / 4;
        
        std::vector<uint8_t> yuv_data(y_size + 2 * uv_size, 128);
        
        // 濉厖 Y 鍒嗛噺锛堟笎鍙橈級
        for (int i = 0; i < y_size; ++i) {
            yuv_data[i] = static_cast<uint8_t>(i % 256);
        }
        
        std::cout << "Created test YUV data: " << width << "x" << height << std::endl;
        
        // 转换为 BGR（YUV420P 格式：Y + U + V）
        const uint8_t* y_data = yuv_data.data();
        const uint8_t* u_data = yuv_data.data() + y_size;
        const uint8_t* v_data = yuv_data.data() + y_size + uv_size;
        
        cv::Mat bgr = converter.Convert(y_data, u_data, v_data, width, height);
        
        if (bgr.empty()) {
            std::cerr << "Conversion failed!" << std::endl;
            return 1;
        }
        
        std::cout << "Converted to BGR: " << bgr.cols << "x" << bgr.rows 
                  << "x" << bgr.channels() << std::endl;
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "Test completed successfully!" << std::endl;
        std::cout << "========================================" << std::endl;
        
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}


