#pragma once

#include "puller/i_puller.h"
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
class ZlmHttpFlvPuller : public IPuller {
public:
    /// @brief 构造函数
    /// @param io_ctx io_context（用于网络操作）
    explicit ZlmHttpFlvPuller(boost::asio::io_context& io_ctx);
    
    /// @brief 析构函数
    ~ZlmHttpFlvPuller() override;
    
    /// @brief 启动拉流
    /// @param url 流地址（格式：http://host/app/stream.flv）
    /// @param seq_cb 序列头回调函数
    /// @param frame_cb 数据回调函数
    /// @return true 成功，false 失败
    bool Start(const std::string& url, 
              SequenceHeaderCallback seq_cb,
              FrameCallback frame_cb) override;
    
    /// @brief 停止拉流
    void Stop() override;
    
    /// @brief 是否正在拉流
    bool IsRunning() const override { return running_; }
    
    /// @brief 设置重连参数
    /// @param delay 重连延迟时间（单位：秒）
    /// @param max_attempts 最大重连次数（-1表示无限重连）
    void SetReconnectParams(int delay, int max_attempts) {
        reconnect_delay_ = delay;
        max_reconnect_attempts_ = max_attempts;
    }
    
private:
    /// @brief 解析 URL
    /// @param url URL
    /// @return true 解析成功
    bool parseUrl(const std::string& url);
    
    /// @brief 连接服务器
    void connect();
    
    /// @brief 发送 HTTP GET 请求
    void sendHttpRequest();
    
    /// @brief 读取 HTTP 响应
    void readHttpResponse();
    
    /// @brief 读取 FLV 头
    void readFlvStream();
    
    /// @brief 读取 FLV 标签
    void readFlvHeader();
    
    /// @brief 读取 PreviousTagSize0
    void readPreviousTagSize0();
    
    /// @brief 异步读取 FLV 标签
    void async_read_tag();
    
    /// @brief 处理 FLV 标签
    /// @param data FLV 标签数据
    /// @param size 标签数据长度
    void handleFlvTag(const uint8_t* data, size_t size);
    
    /// @brief 解析 FLV 标签头
    /// @param data 标签数据
    /// @param size 数据长度
    /// @return 标签类型
    int parseFlvTagHeader(const uint8_t* data, size_t size);
    
    /// @brief 提取 NALU
    /// @param data 数据
    /// @param size 数据长度
    /// @param pts 时间戳
    void extractNalu(const uint8_t* data, size_t size, int64_t pts);
    
    /// @brief 处理重连
    void doReconnect();
    
    /// @brief 重置拉流器状态
    void reset();
    
    /// @brief 缓存关键帧数据（用于网络波动或解码器重置时快速恢复）
    void cacheKeyframe(int codec_id, const uint8_t* data, size_t size);
    
    /// @brief 获取最后的 SPS/PPS 数据（用于快速恢复）
    const std::vector<uint8_t>& getLastSpsPpsH264() const { return last_sps_pps_h264_; }
    const std::vector<uint8_t>& getLastSpsPpsH265() const { return last_vps_sps_pps_h265_; }
    
    // ==================== 成员变量 ====================
    boost::asio::io_context& io_ctx_;
    
    /// @brief TCP socket
    std::unique_ptr<boost::asio::ip::tcp::socket> socket_;
    
    /// @brief FLV 头缓冲区
    std::vector<uint8_t> flv_header_buffer_;
    
    /// @brief HTTP 响应缓冲区（用于读取 HTTP 响应头）
    beast::flat_buffer http_response_buffer_;
    
    /// @brief PreviousTagSize 缓冲区
    std::vector<uint8_t> prev_tag_size_buffer_;
    
    /// @brief 标签头缓冲区
    std::vector<uint8_t> tag_header_buffer_;
    
    /// @brief 当前标签数据
    std::vector<uint8_t> current_tag_data_;
    
    /// @brief 最后的 SPS/PPS 数据（H.264）
    std::vector<uint8_t> last_sps_pps_h264_;
    
    /// @brief 最后的 VPS/SPS/PPS 数据（H.265）
    std::vector<uint8_t> last_vps_sps_pps_h265_;
    
    /// @brief 序列头回调函数
    SequenceHeaderCallback seq_callback_;
    
    /// @brief 数据回调函数
    FrameCallback callback_;
    
    /// @brief 是否包含 FLV 头
    bool has_flv_header_{false};
    
    /// @brief 预期的标签大小
    uint32_t expected_tag_size_{0};
    
    /// @brief 主机名
    std::string host_;
    
    /// @brief 端口号
    std::string port_;
    
    /// @brief 路径
    std::string path_;
    
    /// @brief 重连延迟时间（单位：秒）
    int reconnect_delay_{3};
    
    /// @brief 最大重连次数（-1表示无限重连）
    int max_reconnect_attempts_{-1};
    
    /// @brief 重连次数
    int reconnect_count_{0};
    
    /// @brief 是否已停止
    std::atomic<bool> stopped_{false};
    
    /// @brief 是否正在运行
    std::atomic<bool> running_{false};
    
    /// @brief 接收到的字节数
    std::atomic<uint64_t> bytes_received_{0};
    /// @brief 处理的标签数
    std::atomic<uint64_t> tags_processed_{0};
    /// @brief 交付的帧数
    std::atomic<uint64_t> frames_delivered_{0};
};

