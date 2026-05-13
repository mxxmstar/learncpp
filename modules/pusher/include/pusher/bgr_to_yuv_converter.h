#pragma once

#include <opencv2/opencv.hpp>

class BgrToYuvConverter {
public:
    BgrToYuvConverter() = default;
    ~BgrToYuvConverter() = default;
    
    bool Initialize(int width, int height);
    
    void Convert(const cv::Mat& bgr, uint8_t* yuv_out);
    
    int GetYuvSize() const { return yuv_size_; }
    
private:
    int width_ = 0;
    int height_ = 0;
    int yuv_size_ = 0;
    std::vector<uint8_t> buffer_;
};