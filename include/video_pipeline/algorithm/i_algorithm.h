#pragma once

#include <opencv2/opencv.hpp>
#include <cstdint>

/// @brief 算法接口
class IAlgorithm {
public:
    virtual ~IAlgorithm() = default;
    
    /// @brief 推理函数
    /// @param frame 输入帧
    /// @param timestamp_us 时间戳（微秒）
    virtual void infer(const cv::Mat& frame, int64_t timestamp_us) = 0;
};
