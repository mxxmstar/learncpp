#pragma once

#include "videopipeline/video_pipeline.h"
#include <map>
#include <memory>
#include <mutex>
#include <functional>

/// @brief 流水线统计信息
struct PipelineStats {
    int channel_id = -1;
    bool is_running = false;
    uint64_t frames_received = 0;
    uint64_t frames_decoded = 0;
    uint64_t frames_processed = 0;
    std::string stream_url;
};

/// @brief 视频流水线管理器
/// 单例模式，管理多个 VideoPipeline 实例
class VideoPipelineManager {
public:
    /// @brief 获取单例实例
    static VideoPipelineManager& getInstance();
    
    /// @brief 禁止拷贝和赋值
    VideoPipelineManager(const VideoPipelineManager&) = delete;
    VideoPipelineManager& operator=(const VideoPipelineManager&) = delete;
    
    /// @brief 初始化（必须在首次使用前调用）
    void initialize(boost::asio::io_context& io_ctx);
    
    /// @brief 添加视频流
    /// @param channel_id 通道 ID（唯一标识）
    /// @param config 流水线配置
    /// @return true 成功，false 失败（ID 已存在或配置无效）
    bool addStream(int channel_id, const PipelineConfig& config);
    
    /// @brief 移除视频流
    /// @param channel_id 通道 ID
    /// @return true 成功，false 失败（ID 不存在）
    bool removeStream(int channel_id);
    
    /// @brief 启动指定通道的流水线
    /// @param channel_id 通道 ID
    /// @return true 成功，false 失败
    bool startStream(int channel_id);
    
    /// @brief 停止指定通道的流水线
    /// @param channel_id 通道 ID
    /// @return true 成功，false 失败
    bool stopStream(int channel_id);
    
    /// @brief 启动所有流水线
    void startAllStreams();
    
    /// @brief 停止所有流水线
    void stopAllStreams();
    
    /// @brief 获取指定通道的统计信息
    /// @param channel_id 通道 ID
    /// @return 统计信息
    PipelineStats getStats(int channel_id) const;
    
    /// @brief 获取所有通道的统计信息
    /// @return 所有通道的统计信息列表
    std::vector<PipelineStats> getAllStats() const;
    
    /// @brief 获取所有通道 ID
    /// @return 通道 ID 列表
    std::vector<int> getAllChannelIds() const;
    
    /// @brief 检查通道是否存在
    /// @param channel_id 通道 ID
    /// @return true 存在，false 不存在
    bool hasStream(int channel_id) const;
    
    /// @brief 获取指定通道的流水线指针（用于高级操作）
    /// @param channel_id 通道 ID
    /// @return 流水线指针（可能为 nullptr）
    std::shared_ptr<VideoPipeline> getPipeline(int channel_id);
    
    /// @brief 设置全局帧输出回调（所有通道共用）
    /// @param callback 回调函数
    void setGlobalFrameCallback(VideoPipeline::FrameOutputCallback callback);
    
    /// @brief 设置指定通道的帧输出回调
    /// @param channel_id 通道 ID
    /// @param callback 回调函数
    void setChannelFrameCallback(int channel_id, VideoPipeline::FrameOutputCallback callback);
    
    /// @brief 获取运行中的通道数量
    int getRunningCount() const;
    
    /// @brief 获取总通道数量
    int getTotalCount() const;
    
private:
    /// @brief 私有构造函数（单例模式）
    VideoPipelineManager() = default;
    
    /// @brief 析构函数
    ~VideoPipelineManager();
    
    /// @brief io_context 引用
    boost::asio::io_context* io_ctx_ = nullptr;
    
    /// @brief 流水线映射表（channel_id -> pipeline）
    std::map<int, std::shared_ptr<VideoPipeline>> pipelines_;
    
    /// @brief 互斥锁（保护 pipelines_ 的并发访问）
    mutable std::mutex mutex_;
    
    /// @brief 全局帧输出回调
    VideoPipeline::FrameOutputCallback global_callback_;
};
