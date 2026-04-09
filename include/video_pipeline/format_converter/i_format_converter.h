#pragma once

#include <opencv2/opencv.hpp>

namespace video_pipeline {
namespace format_converter {

/// @brief 格式转换接口（将 VideoFrame 转换为 cv::Mat）
class IFormatConverter {
public:
    virtual ~IFormatConverter() = default;
    
    /// @brief 处理图像帧
    /// @param input 输入帧（右值引用，允许移动）
    /// @return 处理后的帧
    virtual cv::Mat process(cv::Mat&& input) = 0;
};

} // namespace format_converter
} // namespace video_pipeline
