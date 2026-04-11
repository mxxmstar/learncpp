#pragma once

#include "decoder/i_decoder.h"
#include <opencv2/opencv.hpp>

extern "C" {
#include <libswscale/swscale.h>
}

/// @brief OpenCV 格式转换器（将 VideoFrame 转换为 cv::Mat）
/// 这是可选组件，仅在需要使用 OpenCV 时才引入
class OpenCVFormatConverter {
public:
    /// @brief 处理回调类型
    using ProcessedCallback = std::function<void(cv::Mat&& frame, int64_t pts)>;
    
    /// @brief 构造函数
    OpenCVFormatConverter();
    
    /// @brief 析构函数
    ~OpenCVFormatConverter();
    
    /// @brief 处理视频帧（转换为 BGR 格式）
    /// @param frame 输入的视频帧
    /// @param cb 处理后的回调
    void Process(VideoFrame&& frame, ProcessedCallback cb);
    
private:
    /// @brief 创建或获取 SwsContext
    SwsContext* getSwsContext(int src_width, int src_height, int src_format,
                              int dst_width, int dst_height, int dst_format);
    
    // SwsContext 缓存（避免重复创建）
    struct SwsContextCache {
        int src_width = 0;
        int src_height = 0;
        int src_format = -1;
        int dst_width = 0;
        int dst_height = 0;
        int dst_format = -1;
        SwsContext* ctx = nullptr;
        
        ~SwsContextCache() {
            if (ctx) {
                sws_freeContext(ctx);
                ctx = nullptr;
            }
        }
    };
    
    SwsContextCache cache_;
};
