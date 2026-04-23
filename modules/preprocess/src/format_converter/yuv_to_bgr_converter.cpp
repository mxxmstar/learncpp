#include "preprocess/format_converter/yuv_to_bgr_converter.h"
#include "common/log/logmanager.h"



YuvToBgrConverter::YuvToBgrConverter() {
}

YuvToBgrConverter::~YuvToBgrConverter() {
}

bool YuvToBgrConverter::validateInput(const uint8_t* y_data, 
                                      const uint8_t* u_data, 
                                      const uint8_t* v_data,
                                      int width, 
                                      int height) {
    if (!y_data || !u_data || !v_data) {
        LOG_MAIN_WARN_AT("YuvToBgrConverter: null data pointer");
        return false;
    }
    
    if (width <= 0 || height <= 0) {
        LOG_MAIN_WARN_AT("YuvToBgrConverter: invalid dimensions {}x{}", width, height);
        return false;
    }
    
    // YUV420P 要求宽高都是偶数
    if (width % 2 != 0 || height % 2 != 0) {
        LOG_MAIN_WARN_AT("YuvToBgrConverter: dimensions must be even {}x{}", width, height);
        return false;
    }
    
    return true;
}

cv::Mat YuvToBgrConverter::Convert(const uint8_t* y_data, 
                                   const uint8_t* u_data, 
                                   const uint8_t* v_data,
                                   int width, 
                                   int height) {
    // 验证输入
    if (!validateInput(y_data, u_data, v_data, width, height)) {
        return cv::Mat();
    }
    
    try {
        // 计算平面大小
        int y_size = width * height;
        int uv_size = y_size / 4;
        
        // 创建连续的 YUV 数据缓冲区
        std::vector<uint8_t> yuv_data(y_size + uv_size * 2);
        
        // 复制三个平面（注意：使用 linesize 会更准确，但这里假设无填充）
        memcpy(yuv_data.data(), y_data, y_size);              // Y plane
        memcpy(yuv_data.data() + y_size, u_data, uv_size);    // U plane
        memcpy(yuv_data.data() + y_size + uv_size, v_data, uv_size);  // V plane
        
        // 创建 YUV Mat（I420 格式：高度为原高度的 1.5 倍）
        cv::Mat yuv_mat(height * 3/2, width, CV_8UC1, yuv_data.data());
        
        // 转换为 BGR
        cv::Mat bgr_mat;
        cv::cvtColor(yuv_mat, bgr_mat, cv::COLOR_YUV2BGR_I420);
        
        if (bgr_mat.empty()) {
            LOG_MAIN_WARN_AT("YuvToBgrConverter: conversion resulted in empty mat");
            return cv::Mat();
        }
        
        return bgr_mat;
        
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("YuvToBgrConverter: conversion failed: {}", e.what());
        return cv::Mat();
    }
}

std::vector<uint8_t> YuvToBgrConverter::EncodeToJpeg(const cv::Mat& bgr_mat, int quality) {
    if (bgr_mat.empty()) {
        LOG_MAIN_WARN_AT("YuvToBgrConverter: cannot encode empty mat");
        return {};
    }
    
    // 验证质量参数
    if (quality < 1 || quality > 100) {
        LOG_MAIN_WARN_AT("YuvToBgrConverter: invalid quality {}, using default 85", quality);
        quality = 85;
    }
    
    try {
        std::vector<uchar> buf;
        std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, quality};
        
        if (!cv::imencode(".jpg", bgr_mat, buf, params)) {
            LOG_MAIN_WARN_AT("YuvToBgrConverter: JPEG encoding failed");
            return {};
        }
        
        // 转换为 uint8_t vector
        std::vector<uint8_t> jpeg_data(buf.begin(), buf.end());
        
        LOG_MAIN_DEBUG_AT("YuvToBgrConverter: encoded {}x{} to {} bytes", 
                         bgr_mat.cols, bgr_mat.rows, jpeg_data.size());
        
        return jpeg_data;
        
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("YuvToBgrConverter: JPEG encoding failed: {}", e.what());
        return {};
    }
}



