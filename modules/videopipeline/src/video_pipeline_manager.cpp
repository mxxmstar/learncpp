#include "videopipeline/video_pipeline_manager.h"
#include "common/log/logmanager.h"

VideoPipelineManager& VideoPipelineManager::getInstance() {
    static VideoPipelineManager instance;
    return instance;
}

VideoPipelineManager::~VideoPipelineManager() {
    stopAllStreams();
    LOG_MAIN_INFO_AT("VideoPipelineManager destroyed");
}

void VideoPipelineManager::initialize(boost::asio::io_context& io_ctx) {
    std::lock_guard<std::mutex> lock(mutex_);
    io_ctx_ = &io_ctx;
    LOG_MAIN_INFO_AT("VideoPipelineManager initialized");
}

bool VideoPipelineManager::addStream(int channel_id, const PipelineConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 检查 ID 是否已存在
    if (pipelines_.find(channel_id) != pipelines_.end()) {
        LOG_MAIN_WARN_AT("Channel {} already exists", channel_id);
        return false;
    }
    
    // 检查配置有效性
    if (!config.isValid()) {
        LOG_MAIN_ERROR_AT("Invalid pipeline config for channel {}", channel_id);
        return false;
    }
    
    // 检查 io_ctx 是否已初始化
    if (!io_ctx_) {
        LOG_MAIN_ERROR_AT("VideoPipelineManager not initialized");
        return false;
    }
    
    try {
        // 创建流水线
        auto pipeline = std::make_shared<VideoPipeline>(*io_ctx_, config);
        
        // 如果设置了全局回调，自动应用
        if (global_callback_) {
            pipeline->setFrameOutputCallback(global_callback_);
        }
        
        // 添加到映射表
        pipelines_[channel_id] = pipeline;
        
        LOG_MAIN_INFO_AT("Added stream: channel={}, url={}", 
                        channel_id, config.stream_url);
        return true;
    }
    catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("Failed to add stream: {}", e.what());
        return false;
    }
}

bool VideoPipelineManager::removeStream(int channel_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = pipelines_.find(channel_id);
    if (it == pipelines_.end()) {
        LOG_MAIN_WARN_AT("Channel {} does not exist", channel_id);
        return false;
    }
    
    // 先停止流水线
    it->second->stop();
    
    // 从映射表中移除
    pipelines_.erase(it);
    
    LOG_MAIN_INFO_AT("Removed stream: channel={}", channel_id);
    return true;
}

bool VideoPipelineManager::startStream(int channel_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = pipelines_.find(channel_id);
    if (it == pipelines_.end()) {
        LOG_MAIN_WARN_AT("Channel {} does not exist", channel_id);
        return false;
    }
    
    bool success = it->second->start();
    if (success) {
        LOG_MAIN_INFO_AT("Started stream: channel={}", channel_id);
    }
    else {
        LOG_MAIN_ERROR_AT("Failed to start stream: channel={}", channel_id);
    }
    
    return success;
}

bool VideoPipelineManager::stopStream(int channel_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = pipelines_.find(channel_id);
    if (it == pipelines_.end()) {
        LOG_MAIN_WARN_AT("Channel {} does not exist", channel_id);
        return false;
    }
    
    it->second->stop();
    LOG_MAIN_INFO_AT("Stopped stream: channel={}", channel_id);
    return true;
}

void VideoPipelineManager::startAllStreams() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    int started_count = 0;
    for (auto& [channel_id, pipeline] : pipelines_) {
        if (pipeline->start()) {
            started_count++;
        }
    }
    
    LOG_MAIN_INFO_AT("Started {} streams", started_count);
}

void VideoPipelineManager::stopAllStreams() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& [channel_id, pipeline] : pipelines_) {
        pipeline->stop();
    }
    
    LOG_MAIN_INFO_AT("Stopped all streams");
}

PipelineStats VideoPipelineManager::getStats(int channel_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    PipelineStats stats;
    
    auto it = pipelines_.find(channel_id);
    if (it == pipelines_.end()) {
        stats.channel_id = channel_id;
        stats.is_running = false;
        return stats;
    }
    
    stats.channel_id = channel_id;
    stats.is_running = it->second->isRunning();
    stats.frames_received = it->second->getFramesReceived();
    stats.frames_decoded = it->second->getFramesDecoded();
    stats.frames_processed = it->second->getFramesProcessed();
    // TODO: 从 config 中获取 stream_url
    
    return stats;
}

std::vector<PipelineStats> VideoPipelineManager::getAllStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<PipelineStats> all_stats;
    
    for (const auto& [channel_id, pipeline] : pipelines_) {
        PipelineStats stats;
        stats.channel_id = channel_id;
        stats.is_running = pipeline->isRunning();
        stats.frames_received = pipeline->getFramesReceived();
        stats.frames_decoded = pipeline->getFramesDecoded();
        stats.frames_processed = pipeline->getFramesProcessed();
        
        all_stats.push_back(stats);
    }
    
    return all_stats;
}

std::vector<int> VideoPipelineManager::getAllChannelIds() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<int> ids;
    for (const auto& [channel_id, pipeline] : pipelines_) {
        ids.push_back(channel_id);
    }
    
    return ids;
}

bool VideoPipelineManager::hasStream(int channel_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pipelines_.find(channel_id) != pipelines_.end();
}

std::shared_ptr<VideoPipeline> VideoPipelineManager::getPipeline(int channel_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = pipelines_.find(channel_id);
    if (it == pipelines_.end()) {
        return nullptr;
    }
    
    return it->second;
}

void VideoPipelineManager::setGlobalFrameCallback(VideoPipeline::FrameOutputCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    global_callback_ = std::move(callback);
    
    // 应用到所有现有的流水线
    for (auto& [channel_id, pipeline] : pipelines_) {
        pipeline->setFrameOutputCallback(global_callback_);
    }
    
    LOG_MAIN_INFO_AT("Set global frame callback");
}

void VideoPipelineManager::setChannelFrameCallback(int channel_id, 
                                                   VideoPipeline::FrameOutputCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = pipelines_.find(channel_id);
    if (it == pipelines_.end()) {
        LOG_MAIN_WARN_AT("Channel {} does not exist", channel_id);
        return;
    }
    
    it->second->setFrameOutputCallback(std::move(callback));
    LOG_MAIN_INFO_AT("Set frame callback for channel {}", channel_id);
}

int VideoPipelineManager::getRunningCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    int count = 0;
    for (const auto& [channel_id, pipeline] : pipelines_) {
        if (pipeline->isRunning()) {
            count++;
        }
    }
    
    return count;
}

int VideoPipelineManager::getTotalCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<int>(pipelines_.size());
}

