#include "video_pipeline/processor/opencv_processor.h"
#include "log/logmanager.h"

OpenCVProcessor::OpenCVProcessor(const std::vector<std::string>& filters)
    : filters_(filters)
{
    // 初始化滤镜函数映射表
    filter_map_["gaussian_blur"] = [this](cv::Mat&& img) {
        return applyGaussianBlur(std::move(img));
    };
    
    filter_map_["hist_eq"] = [this](cv::Mat&& img) {
        return applyHistogramEqualization(std::move(img));
    };
    
    filter_map_["canny"] = [this](cv::Mat&& img) {
        return applyCannyEdge(std::move(img));
    };
    
    filter_map_["resize"] = [this](cv::Mat&& img) {
        return applyResize(std::move(img));
    };
    
    filter_map_["grayscale"] = [this](cv::Mat&& img) {
        return applyGrayscale(std::move(img));
    };
    
    filter_map_["threshold"] = [this](cv::Mat&& img) {
        return applyThreshold(std::move(img));
    };
    
    filter_map_["median_blur"] = [this](cv::Mat&& img) {
        return applyMedianBlur(std::move(img));
    };
    
    filter_map_["sobel"] = [this](cv::Mat&& img) {
        return applySobel(std::move(img));
    };
    
    filter_map_["laplacian"] = [this](cv::Mat&& img) {
        return applyLaplacian(std::move(img));
    };
    
    filter_map_["morphology"] = [this](cv::Mat&& img) {
        return applyMorphology(std::move(img));
    };
    
    LOG_MAIN_INFO_AT("OpenCVProcessor created with {} filters", filters_.size());
}

OpenCVProcessor::~OpenCVProcessor() {
    LOG_MAIN_INFO_AT("OpenCVProcessor destroyed");
}

cv::Mat OpenCVProcessor::process(cv::Mat&& input) {
    if (input.empty()) {
        LOG_MAIN_WARN_AT("Input frame is empty");
        return cv::Mat();
    }
    
    // 按顺序应用所有滤镜
    cv::Mat result = std::move(input);
    
    for (const auto& filter_name : filters_) {
        auto it = filter_map_.find(filter_name);
        if (it == filter_map_.end()) {
            LOG_MAIN_WARN_AT("Unknown filter: {}", filter_name);
            continue;
        }
        
        // 应用滤镜
        result = it->second(std::move(result));
    }
    
    return result;
}

void OpenCVProcessor::addFilter(const std::string& filter_name) {
    // 检查滤镜是否存在
    if (filter_map_.find(filter_name) == filter_map_.end()) {
        LOG_MAIN_WARN_AT("Cannot add unknown filter: {}", filter_name);
        return;
    }
    
    filters_.push_back(filter_name);
    LOG_MAIN_INFO_AT("Added filter: {}", filter_name);
}

void OpenCVProcessor::clearFilters() {
    filters_.clear();
    LOG_MAIN_INFO_AT("Cleared all filters");
}

void OpenCVProcessor::setGaussianBlurParams(int ksize, double sigmaX) {
    if (ksize <= 0 || ksize % 2 == 0) {
        LOG_MAIN_WARN_AT("Invalid Gaussian kernel size: {} (must be positive odd number)", ksize);
        return;
    }
    gaussian_ksize_ = ksize;
    gaussian_sigma_x_ = sigmaX;
    LOG_MAIN_INFO_AT("Set Gaussian blur params: ksize={}, sigmaX={}", ksize, sigmaX);
}

// ==================== 滤镜实现方法 ====================

cv::Mat OpenCVProcessor::applyGaussianBlur(cv::Mat&& input) {
    cv::Mat output;
    cv::GaussianBlur(input, output, 
                    cv::Size(gaussian_ksize_, gaussian_ksize_),
                    gaussian_sigma_x_);
    return output;
}

cv::Mat OpenCVProcessor::applyHistogramEqualization(cv::Mat&& input) {
    cv::Mat output;
    
    // 如果是彩色图像，先转到 YCrCb，然后对 Y 通道做均衡化
    if (input.channels() == 3) {
        cv::Mat ycrcb;
        cv::cvtColor(input, ycrcb, cv::COLOR_BGR2YCrCb);
        
        std::vector<cv::Mat> channels;
        cv::split(ycrcb, channels);
        
        // 对 Y 通道（亮度）进行直方图均衡化
        cv::equalizeHist(channels[0], channels[0]);
        
        cv::merge(channels, ycrcb);
        cv::cvtColor(ycrcb, output, cv::COLOR_YCrCb2BGR);
    }
    else {
        // 灰度图像直接均衡化
        cv::equalizeHist(input, output);
    }
    
    return output;
}

cv::Mat OpenCVProcessor::applyCannyEdge(cv::Mat&& input) {
    cv::Mat gray, edges;
    
    // 先转灰度
    if (input.channels() == 3) {
        cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);
    }
    else {
        gray = input;
    }
    
    // Canny 边缘检测
    cv::Canny(gray, edges, canny_threshold1_, canny_threshold2_);
    
    // 转回 BGR（便于后续处理）
    cv::cvtColor(edges, edges, cv::COLOR_GRAY2BGR);
    
    return edges;
}

cv::Mat OpenCVProcessor::applyResize(cv::Mat&& input) {
    if (target_width_ <= 0 || target_height_ <= 0) {
        LOG_MAIN_WARN_AT("Invalid target size: {}x{}", target_width_, target_height_);
        return input;
    }
    
    cv::Mat output;
    cv::resize(input, output, cv::Size(target_width_, target_height_), 
               0, 0, cv::INTER_LINEAR);
    return output;
}

cv::Mat OpenCVProcessor::applyGrayscale(cv::Mat&& input) {
    cv::Mat gray;
    
    if (input.channels() == 3) {
        cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);
        // 转回 BGR（保持格式一致）
        cv::cvtColor(gray, gray, cv::COLOR_GRAY2BGR);
    }
    else {
        gray = input;
    }
    
    return gray;
}

cv::Mat OpenCVProcessor::applyThreshold(cv::Mat&& input) {
    cv::Mat gray, output;
    
    // 先转灰度
    if (input.channels() == 3) {
        cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);
    }
    else {
        gray = input;
    }
    
    // 二值化
    cv::threshold(gray, output, threshold_value_, 255, cv::THRESH_BINARY);
    
    // 转回 BGR
    cv::cvtColor(output, output, cv::COLOR_GRAY2BGR);
    
    return output;
}

cv::Mat OpenCVProcessor::applyMedianBlur(cv::Mat&& input) {
    cv::Mat output;
    cv::medianBlur(input, output, median_ksize_);
    return output;
}

cv::Mat OpenCVProcessor::applySobel(cv::Mat&& input) {
    cv::Mat gray, grad_x, grad_y, output;
    
    // 先转灰度
    if (input.channels() == 3) {
        cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);
    }
    else {
        gray = input;
    }
    
    // Sobel 算子
    cv::Sobel(gray, grad_x, CV_16S, 1, 0, 3);
    cv::Sobel(gray, grad_y, CV_16S, 0, 1, 3);
    
    // 合并梯度
    cv::Mat abs_grad_x, abs_grad_y;
    cv::convertScaleAbs(grad_x, abs_grad_x);
    cv::convertScaleAbs(grad_y, abs_grad_y);
    
    cv::addWeighted(abs_grad_x, 0.5, abs_grad_y, 0.5, 0, output);
    
    // 转回 BGR
    cv::cvtColor(output, output, cv::COLOR_GRAY2BGR);
    
    return output;
}

cv::Mat OpenCVProcessor::applyLaplacian(cv::Mat&& input) {
    cv::Mat gray, output;
    
    // 先转灰度
    if (input.channels() == 3) {
        cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);
    }
    else {
        gray = input;
    }
    
    // Laplacian 变换
    cv::Laplacian(gray, output, CV_16S, 3);
    cv::convertScaleAbs(output, output);
    
    // 转回 BGR
    cv::cvtColor(output, output, cv::COLOR_GRAY2BGR);
    
    return output;
}

cv::Mat OpenCVProcessor::applyMorphology(cv::Mat&& input) {
    cv::Mat gray, output;
    
    // 先转灰度
    if (input.channels() == 3) {
        cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);
    }
    else {
        gray = input;
    }
    
    // 形态学操作：开运算（先腐蚀后膨胀）
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
    cv::morphologyEx(gray, output, cv::MORPH_OPEN, kernel);
    
    // 转回 BGR
    cv::cvtColor(output, output, cv::COLOR_GRAY2BGR);
    
    return output;
}
