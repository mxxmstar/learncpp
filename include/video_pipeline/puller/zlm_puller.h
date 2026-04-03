#pragma once

#include "video_pipeline/puller/i_puller.h"
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <atomic>
#include <memory>
#include <thread>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;

/// @brief ZLMediaKit HTTP-FLV 拉流器
/// 从 ZLMediaKit 服务器拉取 HTTP-FLV 流，提取 H.264/H.265 数据包
class ZLMPuller : public IPuller {
public:
    /// @brief 构造函数
    /// @param io_ctx io_context（用于网络操作）
    explicit ZLMPuller(boost::asio::io_context& io_ctx);
    
    /// @brief 析构函数
    ~ZLMPuller() override;
    
    /// @brief 启动拉流
    /// @param url 流地址（格式：http://host/app/stream.flv）
    /// @param cb 数据回调函数
    /// @return true 成功，false 失败
    bool start(const std::string& url, FrameCallback cb) override;
    
    /// @brief 停止拉流
    void stop() override;
    
    /// @brief 是否正在运行
    bool isRunning() const override { return running_; }
    
    /// @brief 设置重连参数
    /// @param delay 重连延迟（秒）
    /// @param max_attempts 最大重连次数（-1=无限重试）
    void setReconnectParams(int delay, int max_attempts) {
        reconnect_delay_ = delay;
        max_reconnect_attempts_ = max_attempts;
    }
    
private:
    /// @brief 解析 URL
    /// @param url 完整 URL
    /// @return true 解析成功
    bool parseUrl(const std::string& url);
    
    /// @brief 连接服务器
    void connect();
    
    /// @brief 发送 HTTP GET 请求
    void sendHttpRequest();
    
    /// @brief 读取 FLV 流
    void readFlvStream();
    
    /// @brief 异步读取 FLV 标签
    void async_read_tag();
    
    /// @brief 处理 FLV 标签
    /// @param tag_data FLV 标签数据
    /// @param tag_size 标签大小
    void handleFlvTag(const uint8_t* tag_data, size_t tag_size);
    
    /// @brief 解析 FLV 标签头
    /// @param data 标签数据
    /// @param size 数据大小
    /// @return 标签类型（0=失败，9=视频，8=音频）
    int parseFlvTagHeader(const uint8_t* data, size_t size);
    
    /// @brief 提取 H.264/H.265 NALU
    /// @param data FLV 视频标签数据（去掉标签头）
    /// @param size 数据大小
    /// @param pts 显示时间戳
    void extractNalu(const uint8_t* data, size_t size, int64_t pts);
    
    /// @brief 重连逻辑
    void doReconnect();
    
    /// @brief 重置状态
    void reset();
    
    // io_context 和 socket
    boost::asio::io_context& io_ctx_;
    std::unique_ptr<boost::asio::ip::tcp::socket> socket_;
    
    // URL 解析结果
    std::string host_;
    std::string port_;
    std::string path_;
    
    // HTTP 缓冲区
    beast::flat_buffer buffer_;
    http::response<http::dynamic_body> response_;
    
    // FLV 解析状态
    std::vector<uint8_t> flv_header_buffer_;  // FLV 头缓冲区（9 字节）
    std::vector<uint8_t> prev_tag_size_buffer_;  // PreviousTagSize0（4 字节）
    std::vector<uint8_t> tag_header_buffer_;  // 标签头缓冲区（11 字节）
    
    bool has_flv_header_ = false;
    uint32_t expected_tag_size_ = 0;
    std::vector<uint8_t> current_tag_data_;
    
    // 回调函数
    FrameCallback callback_;
    
    // 重连配置
    int reconnect_delay_ = 3;  // 秒
    int max_reconnect_attempts_ = -1;  // -1=无限重试
    int reconnect_count_ = 0;
    
    // 运行状态
    std::atomic<bool> running_{false};
    std::atomic<bool> stopped_{false};
    
    // 统计信息
    std::atomic<uint64_t> bytes_received_{0};
    std::atomic<uint64_t> tags_processed_{0};
    std::atomic<uint64_t> frames_delivered_{0};
};
