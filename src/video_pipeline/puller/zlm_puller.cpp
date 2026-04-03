#include "video_pipeline/puller/zlm_puller.h"
#include "log/logmanager.h"
#include <regex>
#include <iostream>

namespace {
    // FLV 标签类型常量
    constexpr uint8_t FLV_TAG_TYPE_AUDIO = 8;
    constexpr uint8_t FLV_TAG_TYPE_VIDEO = 9;
    constexpr uint8_t FLV_TAG_TYPE_SCRIPT = 18;
    
    // FLV 头大小
    constexpr size_t FLV_HEADER_SIZE = 9;
    constexpr size_t FLV_TAG_HEADER_SIZE = 11;
    constexpr size_t PREV_TAG_SIZE_SIZE = 4;
}

ZLMPuller::ZLMPuller(boost::asio::io_context& io_ctx)
    : io_ctx_(io_ctx)
    , socket_(std::make_unique<boost::asio::ip::tcp::socket>(io_ctx))
    , flv_header_buffer_(FLV_HEADER_SIZE)
    , prev_tag_size_buffer_(PREV_TAG_SIZE_SIZE)
    , tag_header_buffer_(FLV_TAG_HEADER_SIZE)
{
}

ZLMPuller::~ZLMPuller() {
    stop();
}

bool ZLMPuller::start(const std::string& url, FrameCallback cb) {
    if (running_) {
        LOG_MAIN_WARN_AT("ZLMPuller already running");
        return false;
    }
    
    // 解析 URL
    if (!parseUrl(url)) {
        LOG_MAIN_ERROR_AT("Failed to parse URL: {}", url);
        return false;
    }
    
    callback_ = std::move(cb);
    stopped_ = false;
    running_ = true;
    
    // 异步连接
    connect();
    
    LOG_MAIN_INFO_AT("ZLMPuller started: host={}, port={}, path={}", 
                    host_, port_, path_);
    return true;
}

void ZLMPuller::stop() {
    if (!running_) {
        return;
    }
    
    stopped_ = true;
    running_ = false;
    
    // 关闭 socket（异步操作会立即返回）
    boost::system::error_code ec;
    socket_->close(ec);
    
    LOG_MAIN_INFO_AT("ZLMPuller stopped. Stats: bytes={}, tags={}, frames={}",
                    bytes_received_.load(),
                    tags_processed_.load(),
                    frames_delivered_.load());
}

bool ZLMPuller::parseUrl(const std::string& url) {
    // 解析 HTTP-FLV URL
    // 格式：http://host:port/app/stream.flv
    std::regex url_regex(R"(http://([^:/]+)(?::(\d+))?(/.+\.flv))");
    std::smatch match;
    
    if (!std::regex_match(url, match, url_regex)) {
        LOG_MAIN_ERROR_AT("Invalid HTTP-FLV URL format: {}", url);
        return false;
    }
    
    host_ = match[1].str();
    port_ = match[2].matched ? match[2].str() : "80";
    path_ = match[3].str();
    
    // 移除开头的斜杠
    if (!path_.empty() && path_[0] == '/') {
        path_ = path_.substr(1);
    }
    
    LOG_MAIN_INFO_AT("Parsed URL: host={}, port={}, path={}", host_, port_, path_);
    return true;
}

void ZLMPuller::connect() {
    if (stopped_) {
        return;
    }
    
    try {
        // 解析主机名
        net::ip::tcp::resolver resolver(io_ctx_);
        auto endpoints = resolver.resolve(host_, port_);
        
        // 异步连接
        boost::asio::async_connect(*socket_, endpoints,
            [this](const boost::system::error_code& ec, const net::ip::tcp::endpoint& endpoint) {
                if (ec) {
                    LOG_MAIN_ERROR_AT("Connect failed: {} (attempt {})", 
                                    ec.message(), reconnect_count_ + 1);
                    doReconnect();
                    return;
                }
                
                LOG_MAIN_INFO_AT("Connected to {}:{}", endpoint.address().to_string(), 
                               endpoint.port());
                
                // 连接成功，发送 HTTP 请求
                sendHttpRequest();
            });
    }
    catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("Connect exception: {}", e.what());
        doReconnect();
    }
}

void ZLMPuller::sendHttpRequest() {
    if (stopped_) {
        return;
    }
    
    try {
        // 构建 HTTP GET 请求
        http::request<http::string_body> req;
        req.method(http::verb::get);
        req.target("/" + path_);
        req.version(11);  // HTTP/1.1
        req.set(http::field::host, host_);
        req.set(http::field::user_agent, "ZLMPuller/1.0");
        req.set(http::field::accept, "*/*");
        req.keep_alive(true);
        
        // 异步发送请求
        http::async_write(*socket_, req,
            [this](const boost::system::error_code& ec, std::size_t bytes_transferred) {
                if (ec) {
                    LOG_MAIN_ERROR_AT("Send request failed: {}", ec.message());
                    doReconnect();
                    return;
                }
                
                LOG_MAIN_INFO_AT("Sent HTTP request ({} bytes)", bytes_transferred);
                
                // 读取响应
                readFlvStream();
            });
    }
    catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("Send request exception: {}", e.what());
        doReconnect();
    }
}

void ZLMPuller::readFlvStream() {
    if (stopped_) {
        return;
    }
    
    // 首先读取 FLV 头（9 字节）
    if (!has_flv_header_) {
        boost::asio::async_read(*socket_,
            boost::asio::buffer(flv_header_buffer_),
            [this](const boost::system::error_code& ec, std::size_t bytes_transferred) {
                if (ec) {
                    if (ec != boost::asio::error::operation_aborted) {
                        LOG_MAIN_ERROR_AT("Read FLV header failed: {}", ec.message());
                        doReconnect();
                    }
                    return;
                }
                
                bytes_received_ += bytes_transferred;
                
                // 验证 FLV 头
                // FLV 头格式：'F''L''V''\x01'\x05\x00\x00\x00\x09
                // 其中 \x05 表示有视频和音频流
                if (flv_header_buffer_[0] != 'F' || flv_header_buffer_[1] != 'L' ||
                    flv_header_buffer_[2] != 'V') {
                    LOG_MAIN_ERROR_AT("Invalid FLV signature");
                    doReconnect();
                    return;
                }
                
                LOG_MAIN_INFO_AT("FLV header validated (version={}, flags={})",
                               flv_header_buffer_[3], flv_header_buffer_[4]);
                
                has_flv_header_ = true;
                
                // 继续读取 PreviousTagSize0
                async_read_tag();
            });
    }
    else {
        // 读取标签
        async_read_tag();
    }
}

void ZLMPuller::async_read_tag() {
    if (stopped_) {
        return;
    }
    
    // 1. 读取标签头（11 字节）
    boost::asio::async_read(*socket_,
        boost::asio::buffer(tag_header_buffer_),
        [this](const boost::system::error_code& ec, std::size_t) {
            if (ec) {
                if (ec != boost::asio::error::operation_aborted) {
                    LOG_MAIN_ERROR_AT("Read tag header failed: {}", ec.message());
                    doReconnect();
                }
                return;
            }
            
            // 解析标签头
            int tag_type = parseFlvTagHeader(tag_header_buffer_.data(), 
                                            tag_header_buffer_.size());
            
            if (tag_type == 0) {
                LOG_MAIN_WARN_AT("Invalid tag type, skipping");
                async_read_tag();  // 继续读下一个标签
                return;
            }
            
            // 计算标签体大小（标签头后 3 字节是大端序的大小）
            expected_tag_size_ = (static_cast<uint32_t>(tag_header_buffer_[5]) << 16) |
                                (static_cast<uint32_t>(tag_header_buffer_[6]) << 8) |
                                static_cast<uint32_t>(tag_header_buffer_[7]);
            
            // 分配缓冲区
            current_tag_data_.resize(expected_tag_size_);
            
            // 2. 读取标签体
            boost::asio::async_read(*socket_,
                boost::asio::buffer(current_tag_data_),
                [this, tag_type](const boost::system::error_code& ec, std::size_t) {
                    if (ec) {
                        if (ec != boost::asio::error::operation_aborted) {
                            LOG_MAIN_ERROR_AT("Read tag body failed: {}", ec.message());
                            doReconnect();
                        }
                        return;
                    }
                    
                    bytes_received_ += current_tag_data_.size();
                    tags_processed_++;
                    
                    // 处理标签
                    handleFlvTag(current_tag_data_.data(), current_tag_data_.size());
                    
                    // 3. 读取 PreviousTagSize（4 字节，可以忽略）
                    boost::asio::async_read(*socket_,
                        boost::asio::buffer(prev_tag_size_buffer_),
                        [this](const boost::system::error_code& ec, std::size_t) {
                            if (ec && ec != boost::asio::error::operation_aborted) {
                                LOG_MAIN_ERROR_AT("Read prev tag size failed: {}", ec.message());
                                doReconnect();
                                return;
                            }
                            
                            bytes_received_ += prev_tag_size_buffer_.size();
                            
                            // 继续读下一个标签
                            async_read_tag();
                        });
                });
        });
}

int ZLMPuller::parseFlvTagHeader(const uint8_t* data, size_t size) {
    if (size < FLV_TAG_HEADER_SIZE) {
        return 0;
    }
    
    int tag_type = data[0];
    
    // 提取时间戳（3 字节 + 1 字节扩展）
    uint32_t timestamp = (static_cast<uint32_t>(data[10]) << 16) |
                        (static_cast<uint32_t>(data[9]) << 8) |
                        static_cast<uint32_t>(data[8]);
    
    // 如果有扩展时间戳（第 4 字节）
    if (data[10] & 0xFF) {
        timestamp |= (static_cast<uint32_t>(data[10]) << 24);
    }
    
    LOG_MAIN_DEBUG_AT("FLV Tag: type={}, timestamp={}ms, size={}",
                     tag_type, timestamp, expected_tag_size_);
    
    return tag_type;
}

void ZLMPuller::handleFlvTag(const uint8_t* data, size_t size) {
    int tag_type = tag_header_buffer_[0];
    
    // 提取 PTS（从标签头）
    uint32_t pts = (static_cast<uint32_t>(tag_header_buffer_[10]) << 16) |
                  (static_cast<uint32_t>(tag_header_buffer_[9]) << 8) |
                  static_cast<uint32_t>(tag_header_buffer_[8]);
    
    if (tag_type == FLV_TAG_TYPE_VIDEO) {
        // 视频标签，提取 NALU
        extractNalu(data, size, pts);
    }
    else if (tag_type == FLV_TAG_TYPE_AUDIO) {
        // 音频标签（暂时忽略，或可以传递给解码器）
        LOG_MAIN_DEBUG_AT("Audio tag: {} bytes @ {}ms", size, pts);
    }
    else if (tag_type == FLV_TAG_TYPE_SCRIPT) {
        // 脚本数据（onMetaData 等）
        LOG_MAIN_DEBUG_AT("Script tag: {} bytes", size);
    }
    else {
        LOG_MAIN_DEBUG_AT("Unknown tag type: {}", tag_type);
    }
}

void ZLMPuller::extractNalu(const uint8_t* data, size_t size, int64_t pts) {
    if (size < 5) {
        LOG_MAIN_WARN_AT("Video tag too small: {} bytes", size);
        return;
    }
    
    // FLV 视频标签格式：
    // [FrameType(4bits)][CodecID(4bits)][PacketType(8bits)][CompositionTime(24bits)][Data...]
    
    uint8_t frame_info = data[0];
    uint8_t packet_type = data[1];
    
    // 提取编解码器 ID
    int codec_id = frame_info & 0x0F;
    
    // 只处理 H.264 (7) 和 H.265 (12)
    if (codec_id != 7 && codec_id != 12) {
        LOG_MAIN_DEBUG_AT("Unsupported codec: {}", codec_id);
        return;
    }
    
    // 跳过 5 字节的标签头
    const uint8_t* nalu_data = data + 5;
    size_t nalu_size = size - 5;
    
    if (nalu_size < 4) {
        return;
    }
    
    // 检查是否是关键帧（packet_type == 0 表示序列头）
    if (packet_type == 0) {
        // 序列头（SPS/PPS），应该先传递给解码器
        LOG_MAIN_INFO_AT("Video sequence header (SPS/PPS): {} bytes @ {}ms", 
                        nalu_size, pts);
    }
    else if (packet_type == 1) {
        // NALU 数据
        // 遍历所有 NALU（每个 NALU 前 4 字节是长度）
        size_t offset = 0;
        while (offset + 4 <= nalu_size) {
            uint32_t nalu_len = (static_cast<uint32_t>(nalu_data[offset]) << 24) |
                               (static_cast<uint32_t>(nalu_data[offset + 1]) << 16) |
                               (static_cast<uint32_t>(nalu_data[offset + 2]) << 8) |
                               static_cast<uint32_t>(nalu_data[offset + 3]);
            
            offset += 4;
            
            if (offset + nalu_len > nalu_size) {
                break;
            }
            
            // 调用回调函数传递 NALU 数据
            if (callback_) {
                callback_(nalu_data + offset, nalu_len, pts);
                frames_delivered_++;
            }
            
            offset += nalu_len;
        }
    }
    else {
        LOG_MAIN_DEBUG_AT("Unknown video packet type: {}", packet_type);
    }
}

void ZLMPuller::doReconnect() {
    if (stopped_) {
        return;
    }
    
    // 检查重连次数
    if (max_reconnect_attempts_ >= 0 && reconnect_count_ >= max_reconnect_attempts_) {
        LOG_MAIN_ERROR_AT("Max reconnect attempts ({}) reached", max_reconnect_attempts_);
        running_ = false;
        return;
    }
    
    reconnect_count_++;
    
    LOG_MAIN_INFO_AT("Reconnecting in {} seconds (attempt {})...",
                    reconnect_delay_, reconnect_count_);
    
    // 重置状态
    reset();
    
    // 延迟重连
    std::this_thread::sleep_for(std::chrono::seconds(reconnect_delay_));
    
    if (!stopped_) {
        connect();
    }
}

void ZLMPuller::reset() {
    has_flv_header_ = false;
    expected_tag_size_ = 0;
    current_tag_data_.clear();
    
    bytes_received_ = 0;
    tags_processed_ = 0;
    frames_delivered_ = 0;
}
