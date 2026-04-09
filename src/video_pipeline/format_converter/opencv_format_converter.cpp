#include "video_pipeline/format_converter/opencv_format_converter.h"
#include "log/logmanager.h"

extern "C" {
#include <libavutil/imgutils.h>
}

namespace video_pipeline {
namespace format_converter {

OpenCVFormatConverter::OpenCVFormatConverter() {
}

OpenCVFormatConverter::~OpenCVFormatConverter() {
}

SwsContext* OpenCVFormatConverter::getSwsContext(int src_width, int src_height, int src_format,
                                                  int dst_width, int dst_height, int dst_format) {
    // 检查是否需要重新创建上下文
    if (cache_.ctx && 
        cache_.src_width == src_width && 
        cache_.src_height == src_height && 
        cache_.src_format == src_format &&
        cache_.dst_width == dst_width && 
        cache_.dst_height == dst_height && 
        cache_.dst_format == dst_format) {
        return cache_.ctx;
    }
    
    // 释放旧的上下文
    if (cache_.ctx) {
        sws_freeContext(cache_.ctx);
        cache_.ctx = nullptr;
    }
    
    // 创建新的上下文
    cache_.ctx = sws_getContext(
        src_width, src_height, static_cast<AVPixelFormat>(src_format),
        dst_width, dst_height, static_cast<AVPixelFormat>(dst_format),
        SWS_BILINEAR, nullptr, nullptr, nullptr);
    
    if (!cache_.ctx) {
        LOG_MAIN_ERROR_AT("Failed to create SwsContext");
        return nullptr;
    }
    
    // 更新缓存
    cache_.src_width = src_width;
    cache_.src_height = src_height;
    cache_.src_format = src_format;
    cache_.dst_width = dst_width;
    cache_.dst_height = dst_height;
    cache_.dst_format = dst_format;
    
    return cache_.ctx;
}

void OpenCVFormatConverter::process(VideoFrame&& frame, ProcessedCallback cb) {
    if (!cb) {
        return;
    }
    
    if (frame.width == 0 || frame.height == 0) {
        LOG_MAIN_WARN_AT("Invalid frame dimensions");
        return;
    }
    
    // 1. 获取 SwsContext（YUV -> BGR）
    SwsContext* sws_ctx = getSwsContext(
        frame.width, frame.height, frame.format,
        frame.width, frame.height, AV_PIX_FMT_BGR24);
    
    if (!sws_ctx) {
        LOG_MAIN_ERROR_AT("Failed to get SwsContext");
        return;
    }
    
    // 2. 分配输出缓冲区
    uint8_t* out_buffer = nullptr;
    int out_linesize[4] = {0};  // ✅ 必须是 4，对应最多 4 个平面
    
    int num_bytes = av_image_get_buffer_size(AV_PIX_FMT_BGR24, 
                                             frame.width, frame.height, 1);
    out_buffer = static_cast<uint8_t*>(av_malloc(num_bytes));
    
    if (!out_buffer) {
        LOG_MAIN_ERROR_AT("Failed to allocate output buffer");
        return;
    }
    
    // 3. 填充图像数据数组
    uint8_t* out_data[4] = {nullptr};  // ✅ 必须是 4
    av_image_fill_arrays(out_data, out_linesize, out_buffer,
                        AV_PIX_FMT_BGR24, frame.width, frame.height, 1);
    
    // 4. 转换图像格式
    sws_scale(sws_ctx, frame.data, frame.linesize, 0,
              frame.height, out_data, out_linesize);
    
    // 5. 创建 OpenCV Mat（深拷贝）
    cv::Mat mat(frame.height, frame.width, CV_8UC3, out_data[0], out_linesize[0]);
    cv::Mat mat_copy = mat.clone();
    
    // 6. 清理
    av_free(out_buffer);
    
    // 7. 调用回调
    cb(std::move(mat_copy), frame.pts);
}

} // namespace format_converter
} // namespace video_pipeline
