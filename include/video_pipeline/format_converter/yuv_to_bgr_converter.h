#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include <cstdint>
#include <functional>

namespace video_pipeline {
namespace format_converter {

/**
 * @brief YUV 到 BGR 转换器
 * 
 * 功能：
 * - 将 FFmpeg 解码的 YUV420P 帧转换为 OpenCV BGR 格式
 * - 支持 JPEG 编码
 */
class YuvToBgrConverter {
public:
    YuvToBgrConverter();
    ~YuvToBgrConverter();
    
    // 禁止拷贝
    YuvToBgrConverter(const YuvToBgrConverter&) = delete;
    YuvToBgrConverter& operator=(const YuvToBgrConverter&) = delete;
    
    /**
     * @brief 将 YUV420P 数据转换为 BGR Mat
     * @param y_data Y 平面数据
     * @param u_data U 平面数据
     * @param v_data V 平面数据
     * @param width 图像宽度
     * @param height 图像高度
     * @return 转换后的 BGR Mat，失败返回空 Mat
     */
    cv::Mat convert(const uint8_t* y_data, 
                   const uint8_t* u_data, 
                   const uint8_t* v_data,
                   int width, 
                   int height);
    
    /**
     * @brief 将 BGR Mat 编码为 JPEG
     * @param bgr_mat BGR 图像
     * @param quality JPEG 质量 (1-100)
     * @return JPEG 数据，失败返回空 vector
     */
    std::vector<uint8_t> encodeToJpeg(const cv::Mat& bgr_mat, int quality = 85);
    
private:
    /// @brief 验证输入参数
    bool validateInput(const uint8_t* y_data, 
                      const uint8_t* u_data, 
                      const uint8_t* v_data,
                      int width, 
                      int height);
};

} // namespace format_converter
} // namespace video_pipeline
