#include <iostream>
#include <opencv2/opencv.hpp>
#include "postprocess/osd/osd_renderer.h"
#include "common/log/logmanager.h"

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "OsdRenderer Test" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    try {
        // 鍒濆鍖栨棩蹇?
        LogManager& log_mgr = LogManager::getInstance();
        log_mgr.Init();
        
        // 鍒涘缓 OSD 娓叉煋鍣?
        OsdRenderer renderer;
        
        // 鍒涘缓娴嬭瘯鍥惧儚
        cv::Mat test_image(480, 640, CV_8UC3, cv::Scalar(100, 150, 200));
        
        std::cout << "Created test image: " << test_image.cols << "x" 
                  << test_image.rows << std::endl;
        
        // 鏋勫缓妫€娴嬫鍒楄〃
        std::vector<std::tuple<int, int, int, int, std::string, float>> detection_boxes;
        detection_boxes.emplace_back(100, 100, 200, 150, "Person", 0.95f);
        detection_boxes.emplace_back(350, 200, 150, 100, "Car", 0.87f);
        
        // 娓叉煋 OSD 淇℃伅
        int channel_id = 1;
        int64_t pts = 1234567;  // 寰
        float fps = 25.5f;
        
        renderer.Render(test_image, channel_id, pts, fps, detection_boxes);
        
        std::cout << "OSD rendered successfully" << std::endl;
        std::cout << "  Channel: " << channel_id << std::endl;
        std::cout << "  PTS: " << pts << " us" << std::endl;
        std::cout << "  FPS: " << fps << std::endl;
        std::cout << "  Detection boxes: " << detection_boxes.size() << std::endl;
        
        // 鏄剧ず缁撴灉锛堝彲閫夛級
        cv::imshow("OSD Test", test_image);
        std::cout << "\nPress any key to close the window..." << std::endl;
        cv::waitKey(0);
        cv::destroyAllWindows();
        
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


