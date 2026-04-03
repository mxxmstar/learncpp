#pragma once

#include "video_pipeline/processor/i_processor.h"
#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <functional>

/// @brief OpenCV 图像处理器
/// 支持多种滤镜和图像增强操作
class OpenCVProcessor : public IProcessor {
public:
    /// @brief 构造函数
    /// @param filters 滤镜列表（按顺序应用）
    explicit OpenCVProcessor(const std::vector<std::string>& filters);
    
    /// @brief 析构函数
    ~OpenCVProcessor() override;
    
    /// @brief 处理图像帧
    /// @param input 输入帧（右值引用）
    /// @return 处理后的帧
    cv::Mat process(cv::Mat&& input) override;
    
    /// @brief 添加滤镜
    /// @param filter_name 滤镜名称
    void addFilter(const std::string& filter_name);
    
    /// @brief 清除所有滤镜
    void clearFilters();
    
    /// @brief 获取当前滤镜列表
    const std::vector<std::string>& getFilters() const { return filters_; }
    
    /// @brief 设置目标尺寸（用于 resize 滤镜）
    void setTargetSize(int width, int height) {
        target_width_ = width;
        target_height_ = height;
    }
    
    /// @brief 设置高斯模糊参数
    void setGaussianBlurParams(int ksize, double sigmaX = 0);
    
    /// @brief 设置 Canny 边缘检测参数
    void setCannyParams(double threshold1, double threshold2);
    
private:
    // ==================== 滤镜实现方法 ====================
    /// @brief 高斯模糊
    cv::Mat applyGaussianBlur(cv::Mat&& input);
    
    /// @brief 直方图均衡化
    cv::Mat applyHistogramEqualization(cv::Mat&& input);
    
    /// @brief Canny 边缘检测
    cv::Mat applyCannyEdge(cv::Mat&& input);
    
    /// @brief 缩放
    cv::Mat applyResize(cv::Mat&& input);
    
    /// @brief 灰度化
    cv::Mat applyGrayscale(cv::Mat&& input);
    
    /// @brief 二值化
    cv::Mat applyThreshold(cv::Mat&& input);
    
    /// @brief 中值滤波
    cv::Mat applyMedianBlur(cv::Mat&& input);
    
    /// @brief Sobel 边缘检测
    cv::Mat applySobel(cv::Mat&& input);
    
    /// @brief Laplacian 变换
    cv::Mat applyLaplacian(cv::Mat&& input);
    
    /// @brief 形态学操作（腐蚀/膨胀）
    cv::Mat applyMorphology(cv::Mat&& input);
    
    // ==================== 成员变量 ====================
    /// @brief 滤镜列表
    std::vector<std::string> filters_;
    
    /// @brief 滤镜函数映射表
    using FilterFunction = std::function<cv::Mat(cv::Mat&&)>;
    std::map<std::string, FilterFunction> filter_map_;
    
    /// @brief 目标宽度（resize 用）
    int target_width_ = 0;
    
    /// @brief 目标高度（resize 用）
    int target_height_ = 0;
    
    /// @brief 高斯模糊核大小
    int gaussian_ksize_ = 5;
    
    /// @brief 高斯模糊 sigmaX
    double gaussian_sigma_x_ = 0;
    
    /// @brief Canny 阈值 1
    double canny_threshold1_ = 50;
    
    /// @brief Canny 阈值 2
    double canny_threshold2_ = 150;
    
    /// @brief 二值化阈值
    double threshold_value_ = 127;
    
    /// @brief 中值滤波核大小
    int median_ksize_ = 3;
};
