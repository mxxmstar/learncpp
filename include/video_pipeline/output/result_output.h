#pragma once

#include "video_pipeline/algorithm/base_algorithm.h"
#include "log/logmanager.h"
#include <string>
#include <fstream>
#include <mutex>

/// @brief 结果输出器接口
class IResultOutput {
public:
    virtual ~IResultOutput() = default;
    
    /// @brief 输出结果
    virtual void output(const AlgorithmResult& result) = 0;
};

/// @brief 日志输出器
class LogOutput : public IResultOutput {
public:
    void output(const AlgorithmResult& result) override {
        // 使用 spdlog 输出
        LOG_MAIN_INFO_AT("{}", result.toString());
    }
};

/// @brief 文件输出器（JSON Lines 格式）
class FileOutput : public IResultOutput {
public:
    explicit FileOutput(const std::string& filename) 
        : file_(filename, std::ios::app) {
        if (!file_.is_open()) {
            throw std::runtime_error("Failed to open file: " + filename);
        }
    }
    
    ~FileOutput() {
        if (file_.is_open()) {
            file_.close();
        }
    }
    
    void output(const AlgorithmResult& result) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // JSON Lines 格式：每行一个 JSON 对象
        file_ << "{\"channel\":" << result.channel_id
              << ",\"ts\":" << result.timestamp_us
              << ",\"type\":\"" << result.algorithm_type << "\""
              << ",\"conf\":" << result.confidence
              << ",\"data\":" << result.result_data
              << "}\n";
        
        file_.flush();  // 立即写入磁盘
    }
    
private:
    std::ofstream file_;
    std::mutex mutex_;
};

/// @brief 控制台输出器
class ConsoleOutput : public IResultOutput {
public:
    void output(const AlgorithmResult& result) override {
        static int last_channel = -1;
        static int64_t last_ts = 0;
        static int count = 0;
        
        // 每 30 帧或通道变化时打印一次
        if (result.channel_id != last_channel || ++count % 30 == 0) {
            printf("\033[32m[Algorithm] %s\033[0m\n", result.toString().c_str());
            fflush(stdout);
            
            last_channel = result.channel_id;
            last_ts = result.timestamp_us;
            count = 0;
        }
    }
};
