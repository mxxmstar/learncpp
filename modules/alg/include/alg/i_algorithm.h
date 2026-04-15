#pragma once

#include <opencv2/opencv.hpp>
#include <functional>
#include <string>

/// @brief 算法处理结果
struct AlgorithmResult {
    int channel_id = -1;           // 通道 ID
    int64_t timestamp_us = 0;      // 时间戳（微秒）
    std::string algorithm_type;    // 算法类型
    std::string result_data;       // 结果数据（JSON 格式）
    float confidence = 0.0f;       // 置信度
    cv::Rect detection_box;        // 检测框（如果有）
    
    /// @brief 转换为字符串
    std::string toString() const {
        char buf[256];
        snprintf(buf, sizeof(buf), 
                "[Channel=%d, TS=%ldms, Type=%s, Conf=%.2f, Box=(%d,%d,%d,%d)]",
                channel_id, timestamp_us / 1000, algorithm_type.c_str(),
                confidence, detection_box.x, detection_box.y,
                detection_box.width, detection_box.height);
        return std::string(buf);
    }
};

/// @brief 基础算法接口
class IAlgorithm {
public:
    virtual ~IAlgorithm() = default;
    
    /// @brief 处理帧并返回结果
    /// @param frame 输入帧
    /// @param channel_id 通道 ID
    /// @param pts 时间戳
    /// @return 算法结果
    virtual AlgorithmResult process(cv::Mat& frame, int channel_id, int64_t pts) = 0;
    
    /// @brief 获取算法名称
    virtual std::string getName() const = 0;
};

/// @brief 空算法实现（用于测试）
class NullAlgorithm : public IAlgorithm {
public:
    AlgorithmResult process(cv::Mat& frame, int channel_id, int64_t pts) override {
        AlgorithmResult result;
        result.channel_id = channel_id;
        result.timestamp_us = pts * 1000;  // ms -> us
        result.algorithm_type = "null";
        result.result_data = "{}";
        result.confidence = 1.0f;
        
        // 简单打印信息
        static int frame_count = 0;
        if (++frame_count % 30 == 0) {
            printf("[NullAlgo] Frame %d: %dx%d\n", 
                   frame_count, frame.cols, frame.rows);
        }
        
        return result;
    }
    
    std::string getName() const override { return "NullAlgorithm"; }
};

/// @brief 运动检测算法（简单的帧差法）
class MotionDetectionAlgorithm : public IAlgorithm {
public:
    MotionDetectionAlgorithm() = default;
    
    AlgorithmResult process(cv::Mat& frame, int channel_id, int64_t pts) override {
        AlgorithmResult result;
        result.channel_id = channel_id;
        result.timestamp_us = pts * 1000;
        result.algorithm_type = "motion_detection";
        
        // 转灰度
        cv::Mat gray;
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
        
        // 如果是第一帧，保存为背景
        if (background_.empty()) {
            background_ = gray.clone();
            result.result_data = "{\"motion\": false}";
            result.confidence = 0.0f;
            return result;
        }
        
        // 帧差法检测运动
        cv::Mat diff;
        cv::absdiff(background_, gray, diff);
        cv::threshold(diff, diff, 25, 255, cv::THRESH_BINARY);
        
        // 计算运动区域比例
        double motion_ratio = cv::countNonZero(diff) / (double)diff.total();
        
        result.result_data = "{\"motion\": " + std::to_string(motion_ratio > 0.05) + 
                            ", \"ratio\": " + std::to_string(motion_ratio) + "}";
        result.confidence = static_cast<float>(motion_ratio);
        
        // 如果运动超过阈值，更新背景
        if (motion_ratio > 0.05) {
            background_ = gray.clone();
        }
        
        // 每 30 帧打印一次
        static int frame_count = 0;
        if (++frame_count % 30 == 0) {
            printf("[MotionDetect] Channel=%d, Motion=%.2f%%\n", 
                   channel_id, motion_ratio * 100);
        }
        
        return result;
    }
    
    std::string getName() const override { return "MotionDetection"; }
    
private:
    cv::Mat background_;  // 背景帧
};
