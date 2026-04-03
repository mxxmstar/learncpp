#include <iostream>
#include <opencv2/opencv.hpp>
#include "video_pipeline/processor/opencv_processor.h"
#include "log/logmanager.h"

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "OpenCVProcessor Test" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    try {
        // 初始化日志
        LogManager& log_mgr = LogManager::getInstance();
        log_mgr.Init();
        
        // 创建一个测试图像（彩色渐变）
        cv::Mat test_image(480, 640, CV_8UC3);
        
        // 生成渐变色
        for (int y = 0; y < test_image.rows; ++y) {
            for (int x = 0; x < test_image.cols; ++x) {
                test_image.at<cv::Vec3b>(y, x) = cv::Vec3b(
                    static_cast<uchar>(x * 255 / test_image.cols),
                    static_cast<uchar>(y * 255 / test_image.rows),
                    128
                );
            }
        }
        
        std::cout << "Created test image: " << test_image.cols << "x" 
                  << test_image.rows << "x" << test_image.channels() << std::endl;
        
        // 测试 1：单个滤镜 - 高斯模糊
        std::cout << "\n--- Test 1: Gaussian Blur ---" << std::endl;
        OpenCVProcessor blur_processor({"gaussian_blur"});
        auto blurred = blur_processor.process(test_image.clone());
        std::cout << "Blurred image: " << blurred.cols << "x" 
                  << blurred.rows << "x" << blurred.channels() << std::endl;
        
        // 测试 2：滤镜链 - 灰度化 + Canny 边缘检测
        std::cout << "\n--- Test 2: Grayscale + Canny Edge ---" << std::endl;
        OpenCVProcessor edge_processor({"grayscale", "canny"});
        auto edges = edge_processor.process(test_image.clone());
        std::cout << "Edge image: " << edges.cols << "x" 
                  << edges.rows << "x" << edges.channels() << std::endl;
        
        // 测试 3：滤镜链 - 直方图均衡化 + 中值滤波
        std::cout << "\n--- Test 3: Histogram Equalization + Median Blur ---" << std::endl;
        OpenCVProcessor enhance_processor({"hist_eq", "median_blur"});
        auto enhanced = enhance_processor.process(test_image.clone());
        std::cout << "Enhanced image: " << enhanced.cols << "x" 
                  << enhanced.rows << "x" << enhanced.channels() << std::endl;
        
        // 测试 4：动态添加滤镜
        std::cout << "\n--- Test 4: Dynamic Filter Addition ---" << std::endl;
        OpenCVProcessor dynamic_processor({});
        dynamic_processor.addFilter("grayscale");
        dynamic_processor.addFilter("threshold");
        auto processed = dynamic_processor.process(test_image.clone());
        std::cout << "Dynamic processed image: " << processed.cols << "x" 
                  << processed.rows << "x" << processed.channels() << std::endl;
        
        // 测试 5：清除滤镜
        std::cout << "\n--- Test 5: Clear Filters ---" << std::endl;
        dynamic_processor.clearFilters();
        dynamic_processor.addFilter("sobel");
        auto sobel_result = dynamic_processor.process(test_image.clone());
        std::cout << "Sobel result: " << sobel_result.cols << "x" 
                  << sobel_result.rows << "x" << sobel_result.channels() << std::endl;
        
        // 测试 6：自定义参数
        std::cout << "\n--- Test 6: Custom Parameters ---" << std::endl;
        OpenCVProcessor custom_processor({"gaussian_blur", "resize"});
        custom_processor.setGaussianBlurParams(7, 2.0);
        custom_processor.setTargetSize(320, 240);
        auto custom_result = custom_processor.process(test_image.clone());
        std::cout << "Custom result: " << custom_result.cols << "x" 
                  << custom_result.rows << "x" << custom_result.channels() << std::endl;
        
        // 测试 7：空输入处理
        std::cout << "\n--- Test 7: Empty Input Handling ---" << std::endl;
        cv::Mat empty_img;
        auto empty_result = edge_processor.process(std::move(empty_img));
        std::cout << "Empty input handled: " << (empty_result.empty() ? "Yes" : "No") << std::endl;
        
        // 测试 8：所有滤镜类型
        std::cout << "\n--- Test 8: All Filter Types ---" << std::endl;
        std::vector<std::string> all_filters = {
            "gaussian_blur",
            "hist_eq",
            "canny",
            "resize",
            "grayscale",
            "threshold",
            "median_blur",
            "sobel",
            "laplacian",
            "morphology"
        };
        
        for (const auto& filter : all_filters) {
            OpenCVProcessor single_processor({filter});
            if (filter == "resize") {
                single_processor.setTargetSize(320, 240);
            }
            
            auto result = single_processor.process(test_image.clone());
            std::cout << "Filter '" << filter << "' -> " 
                      << result.cols << "x" << result.rows << "x" 
                      << result.channels() << std::endl;
        }
        
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
