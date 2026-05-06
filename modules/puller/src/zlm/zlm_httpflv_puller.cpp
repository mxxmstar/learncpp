#include "puller/zlm/zlm_httpflv_puller.h"
#include "common/log/logmanager.h"
#include <regex>
#include <iostream>
/*
flv 流结构：
[FLV Header (9 bytes)] [PreviousTagSize0 (4 bytes)] 
[Tag1 Header (11 bytes)] [Tag1 Data] [PreviousTagSize1 (4 bytes)]
[Tag2 Header (11 bytes)] [Tag2 Data] [PreviousTagSize2 (4 bytes)]
 */ 


namespace {
    // FLV 标签类型常量
    constexpr uint8_t FLV_TAG_TYPE_AUDIO = 8;   // 音频标签
    constexpr uint8_t FLV_TAG_TYPE_VIDEO = 9;   // 视频标签
    constexpr uint8_t FLV_TAG_TYPE_SCRIPT = 18; // 脚本标签（Metadata）
    
    // FLV 头大小
    constexpr size_t FLV_HEADER_SIZE = 9;    // FLV 头大小
    constexpr size_t FLV_TAG_HEADER_SIZE = 11;  // FLV 标签头大小
    constexpr size_t PREV_TAG_SIZE_SIZE = 4;  // 上一个标签大小字段
}

ZlmHttpFlvPuller::ZlmHttpFlvPuller(boost::asio::io_context& io_ctx)
    : io_ctx_(io_ctx)
    , socket_(std::make_unique<boost::asio::ip::tcp::socket>(io_ctx))
    , flv_header_buffer_(FLV_HEADER_SIZE)
    , prev_tag_size_buffer_(PREV_TAG_SIZE_SIZE)
    , tag_header_buffer_(FLV_TAG_HEADER_SIZE)
{
}

ZlmHttpFlvPuller::~ZlmHttpFlvPuller() {
    Stop();
}

bool ZlmHttpFlvPuller::Start(const std::string& url, SequenceHeaderCallback seq_cb, FrameCallback frame_cb) {
    if (running_) {
        LOG_MAIN_WARN_AT("ZlmHttpFlvPuller already running");
        return false;
    }
    
    // 解析 URL
    if (!parseUrl(url)) {
        LOG_MAIN_ERROR_AT("Failed to parse URL: {}", url);
        return false;
    }
    
    seq_callback_ = std::move(seq_cb);
    callback_ = std::move(frame_cb);
    stopped_ = false;
    running_ = true;
    
    // 异步连接
    connect();
    
    LOG_MAIN_INFO_AT("ZlmHttpFlvPuller started: host={}, port={}, path={}", 
                    host_, port_, path_);
    return true;
}

void ZlmHttpFlvPuller::Stop() {
    if (!running_) {
        return;
    }
    
    stopped_ = true;
    running_ = false;
    
    // 关闭 socket（异步操作会立即返回）
    boost::system::error_code ec;
    socket_->close(ec);
    
    LOG_MAIN_INFO_AT("ZlmHttpFlvPuller stopped. Stats: bytes={}, tags={}, frames={}",
                    bytes_received_.load(),
                    tags_processed_.load(),
                    frames_delivered_.load());
}

bool ZlmHttpFlvPuller::parseUrl(const std::string& url) {
    // 解析 HTTP-FLV URL
    // 格式：http://host:port/app/stream.live.flv
    // 示例：http://127.0.0.1:80/live/proxy_cam1.live.flv
    std::regex url_regex(R"(http://([^:/]+)(?::(\d+))?(/.+\.live\.flv))");
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

void ZlmHttpFlvPuller::connect() {
    if (stopped_) {
        return;
    }
    
    try {
        // 解析主机名（resolver 必须在异步操作完成前保持存活）
        auto resolver = std::make_shared<net::ip::tcp::resolver>(io_ctx_);
        auto endpoints = resolver->resolve(host_, port_);
        
        // 异步连接
        boost::asio::async_connect(*socket_, endpoints,
            [this, resolver](const boost::system::error_code& ec, const net::ip::tcp::endpoint& endpoint) {
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

void ZlmHttpFlvPuller::sendHttpRequest() {
    if (stopped_) {
        return;
    }
    
    try {
        // 构建 HTTP GET 请求（使用 shared_ptr 确保生命周期）
        auto req = std::make_shared<http::request<http::string_body>>();
        req->method(http::verb::get);
        req->target("/" + path_);
        req->version(11);  // HTTP/1.1
        req->set(http::field::host, host_);
        req->set(http::field::user_agent, "ZlmHttpFlvPuller/1.0");
        req->set(http::field::accept, "*/*");
        req->keep_alive(true);    // 保持连接
        
        // 异步发送请求
        http::async_write(*socket_, *req,
            [this, req](const boost::system::error_code& ec, std::size_t bytes_transferred) {
                if (ec) {
                    LOG_MAIN_ERROR_AT("Send request failed: {}", ec.message());
                    doReconnect();
                    return;
                }
                
                /*LOG_MAIN_INFO_AT("Sent HTTP request ({} bytes)", bytes_transferred);
                LOG_MAIN_INFO_AT("Request:{}", req->target());*/
                // 读取 HTTP 响应头
                readHttpResponse();
            });
    }
    catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("Send request exception: {}", e.what());
        doReconnect();
    }
}

void ZlmHttpFlvPuller::readHttpResponse() {
    if (stopped_) {
        return;
    }
    
    try {
        // 使用 async_read_until 读取直到 \r\n\r\n
        boost::asio::async_read_until(*socket_,
            http_response_buffer_,
            "\r\n\r\n",
            [this](const boost::system::error_code& ec, std::size_t bytes_transferred) {
                if (ec) {
                    LOG_MAIN_ERROR_AT("Read HTTP response failed: {}", ec.message());
                    doReconnect();
                    return;
                }
                
                // 安全检查：确保有数据可读
                if (bytes_transferred == 0) {
                    LOG_MAIN_ERROR_AT("HTTP response is empty");
                    doReconnect();
                    return;
                }

                // 查看收到的 HTTP 响应数据
                try {
                    // 使用 beast::buffers_to_string 安全转换
                    auto const& buf = http_response_buffer_.data();
                    std::string response = beast::buffers_to_string(buf);
                    
                    LOG_MAIN_INFO_AT("HTTP Response received ({} bytes):", bytes_transferred);
                }
                catch (const std::exception& e) {
                    LOG_MAIN_WARN_AT("Failed to parse HTTP response: {}", e.what());
                }

                // 从缓冲区中移除已读取的 HTTP 响应头
                http_response_buffer_.consume(bytes_transferred);

                // 开始读取 FLV 流
                readFlvStream();

            });
    }
    catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("Read HTTP response exception: {}", e.what());
        doReconnect();
    }
}

void ZlmHttpFlvPuller::readFlvStream() {
    if (stopped_) {
        return;
    }
    
    // 首先读取 FLV 头（9 字节）
    if (!has_flv_header_) {
        readFlvHeader();
    }
    else {
        // 读取标签
        async_read_tag();
    }
}

void ZlmHttpFlvPuller::readFlvHeader() {
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
            // 其中 \x01 表示 FLV 版本, 通常为 0x01
            // 其中 \x05 表示有视频和音频流（第二位为 1 表示有音频流, 第0位为 1 表示有视频流）
            // 其中 \x00 x00 x00 x09 数据偏移量, 通常为 0x09
            if (flv_header_buffer_[0] != 'F' || flv_header_buffer_[1] != 'L' ||
                flv_header_buffer_[2] != 'V') {
                LOG_MAIN_ERROR_AT("Invalid FLV signature: {} {} {}", flv_header_buffer_[0], flv_header_buffer_[1], flv_header_buffer_[2]);
                doReconnect();
                return;
            }
            
            LOG_MAIN_INFO_AT("FLV header validated (version={}, flags={})",
                           flv_header_buffer_[3], flv_header_buffer_[4]);
            
            has_flv_header_ = true;
            
            // 读取 PreviousTagSize0
            readPreviousTagSize0();
        });
}

void ZlmHttpFlvPuller::readPreviousTagSize0() {
    boost::asio::async_read(*socket_,
        boost::asio::buffer(prev_tag_size_buffer_),
        [this](const boost::system::error_code& ec, std::size_t) {
            if (ec) {
                if (ec != boost::asio::error::operation_aborted) {
                    LOG_MAIN_ERROR_AT("Read PreviousTagSize0 failed: {}", ec.message());
                    doReconnect();
                }
                return;
            }
            
            bytes_received_ += prev_tag_size_buffer_.size();
            
            // PreviousTagSize0 应该为 0
            uint32_t prev_size = (static_cast<uint32_t>(prev_tag_size_buffer_[0]) << 24) |
                                (static_cast<uint32_t>(prev_tag_size_buffer_[1]) << 16) |
                                (static_cast<uint32_t>(prev_tag_size_buffer_[2]) << 8) |
                                static_cast<uint32_t>(prev_tag_size_buffer_[3]);
            
            if (prev_size != 0) {
                LOG_MAIN_WARN_AT("PreviousTagSize0 is not 0: {}", prev_size);
            }
            else {
                LOG_MAIN_DEBUG_AT("PreviousTagSize0 = 0 (expected)");
            }
            
            // 开始读取第一个标签
            async_read_tag();
        });
}

void ZlmHttpFlvPuller::async_read_tag() {
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
            
            /*
                Offset  Size  Description
                0       1     Tag Type (8=音频，9=视频，18=脚本)
                1       3     Data Size (大端序)
                4       3     Timestamp (大端序，单位毫秒)
                7       1     Timestamp Extended (高 8 位)
                8       3     Stream ID (通常为 0)
            */

            // 解析标签头
            //LOG_MAIN_INFO_AT("tag header size: {}", tag_header_buffer_.size());            
            int tag_type = parseFlvTagHeader(tag_header_buffer_.data(), 
                                            tag_header_buffer_.size());
            
            if (tag_type == 0) {
                LOG_MAIN_WARN_AT("Invalid tag type, skipping");
                async_read_tag();  // 继续读下一个标签
                return;
            }
            
            // 计算标签体大小（标签头后 3 字节是大端序的大小）
            expected_tag_size_ = (static_cast<uint32_t>(tag_header_buffer_[1]) << 16) |
                                (static_cast<uint32_t>(tag_header_buffer_[2]) << 8) |
                                static_cast<uint32_t>(tag_header_buffer_[3]);
            
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

int ZlmHttpFlvPuller::parseFlvTagHeader(const uint8_t* data, size_t size) {
    if (size < FLV_TAG_HEADER_SIZE) {
        return 0;
    }
    
    std::string hex_str;
    for (size_t i = 0; i < FLV_TAG_HEADER_SIZE && i < size; ++i) {
        char buf[4];
        snprintf(buf, sizeof(buf), "%02X ", data[i]);
        hex_str += buf;
    }
    //LOG_MAIN_DEBUG_AT("{}", hex_str);

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

void ZlmHttpFlvPuller::handleFlvTag(const uint8_t* data, size_t size) {
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
        //LOG_MAIN_DEBUG_AT("Audio tag: {} bytes @ {}ms", size, pts);
    }
    else if (tag_type == FLV_TAG_TYPE_SCRIPT) {
        // 脚本数据（onMetaData 等）
        LOG_MAIN_DEBUG_AT("Script tag: {} bytes", size);
    }
    else {
        LOG_MAIN_DEBUG_AT("Unknown tag type: {}", tag_type);
    }
}

void ZlmHttpFlvPuller::extractNalu(const uint8_t* data, size_t size, int64_t pts) {
    if (size < 5) {
        LOG_MAIN_WARN_AT("Video tag too small: {} bytes", size);
        return;
    }
    
    // FLV 视频标签格式：
    // Offset  Size  Description
    // 0       1     FrameInfo (高 4 位=帧类型，低 4 位=编解码器 ID)
    // 1       1     PacketType (0=序列头，1=NALU，2=结束标记)
    // 2       3     CompositionTime (24 位，DTS 与 PTS 的差值)
    // 5       ...   NALU 数据（每个 NALU 前 4 字节是长度）
    // [FrameType(4bits)][CodecID(4bits)][PacketType(8bits)][CompositionTime(24bits)][Data...]
    
    uint8_t frame_info = data[0];
    uint8_t packet_type = data[1];
    
    // 提取编解码器 ID
    int codec_id = frame_info & 0x0F;
    
    // 只处理 H.264 (7) 和 H.265 (12)
    // TODO: 添加其它编码格式的处理 AV1 (13), VP8 (100), VP9 (101) 等
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
        // 序列头（SPS/PPS）- 这是完整的 AVCC 格式容器，直接传递给解码器
        LOG_MAIN_INFO_AT("Video sequence header (AVCC format): {} bytes @ {}ms", 
                        nalu_size, pts);
        
        // 缓存关键帧数据（用于网络波动或解码器重置时快速恢复）
        cacheKeyframe(codec_id, nalu_data, nalu_size);
        
        // 通过序列头回调传递（新接口）
        if (seq_callback_) {
            seq_callback_(codec_id, nalu_data, nalu_size);
        } else if (callback_) {
            // 兼容旧接口：通过普通回调传递，使用特殊PTS标记
            callback_(nalu_data, nalu_size, -1);
        }
        frames_delivered_++;
    }
    else if (packet_type == 1) {
        // NALU 数据
        // 遍历所有 NALU（每个 NALU 前 4 字节是长度）
        size_t offset = 0;
        while (offset + 4 <= nalu_size) {
            // 【调试】打印前几个字节的十六进制
            static bool first_nalu_debugged = false;
            if (!first_nalu_debugged && offset == 0) {
                std::string hex_str;
                for (size_t i = 0; i < std::min(static_cast<size_t>(16), nalu_size); ++i) {
                    char buf[4];
                    snprintf(buf, sizeof(buf), "%02X ", nalu_data[i]);
                    hex_str += buf;
                }
                LOG_MAIN_DEBUG_AT("First NALU data (hex): {}", hex_str);
                first_nalu_debugged = true;
            }
            
            uint32_t nalu_len = (static_cast<uint32_t>(nalu_data[offset]) << 24) |
                               (static_cast<uint32_t>(nalu_data[offset + 1]) << 16) |
                               (static_cast<uint32_t>(nalu_data[offset + 2]) << 8) |
                               static_cast<uint32_t>(nalu_data[offset + 3]);
            
            offset += 4;
            
            if (offset + nalu_len > nalu_size) {
                LOG_MAIN_WARN_AT("Invalid NAL unit size ({} > {}). Offset={}, remaining={}",
                                nalu_len, nalu_size - offset + 4, offset - 4, nalu_size - offset + 4);
                break;
            }
            
            // 【调试】检查 NALU 类型
            if (nalu_len >= 1) {
                uint8_t nalu_type = nalu_data[offset] & 0x1F;
                
                // 【验证】检查 NALU 长度是否合理
                static const size_t MAX_NALU_SIZE = 500 * 1024;  // 500KB 上限
                if (nalu_len > MAX_NALU_SIZE) {
                    LOG_MAIN_WARN_AT("NALU too large: {} bytes (type={}, pts={}). Skipping.",
                                    nalu_len, nalu_type, pts);
                    break;  // 跳过剩余的 NALU
                }
                
                static int debug_count = 0;
                if (debug_count < 5 || nalu_type == 5) {  // 打印前5个和所有IDR帧
                    LOG_MAIN_DEBUG_AT("NALU: type={}, len={}, offset={}, pts={}",
                                     nalu_type, nalu_len, offset - 4, pts);
                    if (debug_count < 5) debug_count++;
                }
            }
            
            // 调用回调函数传递 NALU 数据
            // 【重要】FFmpeg 使用 AVCC 格式初始化解码器，需要传递带长度前缀的数据
            if (callback_) {
                // 传递完整的 AVCC 格式：[4字节长度][NALU数据]
                callback_(nalu_data + offset - 4, nalu_len + 4, pts);
                frames_delivered_++;
            }
            
            offset += nalu_len;
        }
    }
    else {
        LOG_MAIN_DEBUG_AT("Unknown video packet type: {}", packet_type);
    }
}

void ZlmHttpFlvPuller::doReconnect() {
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

void ZlmHttpFlvPuller::reset() {
    has_flv_header_ = false;
    expected_tag_size_ = 0;
    current_tag_data_.clear();
    
    bytes_received_ = 0;
    tags_processed_ = 0;
    frames_delivered_ = 0;
}

void ZlmHttpFlvPuller::cacheKeyframe(int codec_id, const uint8_t* data, size_t size) {
    if (codec_id == 7) {
        last_sps_pps_h264_.assign(data, data + size);
    } else if (codec_id == 12) {
        last_vps_sps_pps_h265_.assign(data, data + size);
    }
}