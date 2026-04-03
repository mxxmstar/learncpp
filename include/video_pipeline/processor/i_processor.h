#pragma once

#include <opencv2/opencv.hpp>

/// @brief 处理器接口
class IProcessor {
public:
    virtual ~IProcessor() = default;
    
    /// @brief 处理图像帧
    /// @param input 输入帧（右值引用，允许移动）
    /// @return 处理后的帧
    virtual cv::Mat process(cv::Mat&& input) = 0;
};
