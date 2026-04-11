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

/// @brief ZLMediaKit HTTP-FLV 鎷夋祦鍣?
/// 浠?ZLMediaKit 鏈嶅姟鍣ㄦ媺鍙?HTTP-FLV 娴侊紝鎻愬彇 H.264/H.265 鏁版嵁鍖?
class ZlmHttpFlvPuller : public IPuller {
public:
    /// @brief 鏋勯€犲嚱鏁?
    /// @param io_ctx io_context锛堢敤浜庣綉缁滄搷浣滐級
    explicit ZlmHttpFlvPuller(boost::asio::io_context& io_ctx);
    
    /// @brief 鏋愭瀯鍑芥暟
    ~ZlmHttpFlvPuller() override;
    
    /// @brief 鍚姩鎷夋祦
    /// @param url 娴佸湴鍧€锛堟牸寮忥細http://host/app/stream.flv锛?
    /// @param seq_cb 搴忓垪澶村洖璋冨嚱鏁?
    /// @param frame_cb 鏁版嵁鍥炶皟鍑芥暟
    /// @return true 鎴愬姛锛宖alse 澶辫触
    bool Start(const std::string& url, 
              SequenceHeaderCallback seq_cb,
              FrameCallback frame_cb) override;
    
    /// @brief 鍋滄鎷夋祦
    void Stop() override;
    
    /// @brief 鏄惁姝ｅ湪杩愯
    bool IsRunning() const override { return running_; }
    
    /// @brief 璁剧疆閲嶈繛鍙傛暟
    /// @param delay 閲嶈繛寤惰繜锛堢锛?
    /// @param max_attempts 鏈€澶ч噸杩炴鏁帮紙-1=鏃犻檺閲嶈瘯锛?
    void SetReconnectParams(int delay, int max_attempts) {
        reconnect_delay_ = delay;
        max_reconnect_attempts_ = max_attempts;
    }
    
private:
    /// @brief 瑙ｆ瀽 URL
    /// @param url 瀹屾暣 URL
    /// @return true 瑙ｆ瀽鎴愬姛
    bool parseUrl(const std::string& url);
    
    /// @brief 杩炴帴鏈嶅姟鍣?
    void connect();
    
    /// @brief 鍙戦€?HTTP GET 璇锋眰
    void sendHttpRequest();
    
    /// @brief 璇诲彇 HTTP 鍝嶅簲澶?
    void readHttpResponse();
    
    /// @brief 璇诲彇 FLV 娴?
    void readFlvStream();
    
    /// @brief 璇诲彇 FLV 澶?
    void readFlvHeader();
    
    /// @brief 璇诲彇 PreviousTagSize0
    void readPreviousTagSize0();
    
    /// @brief 寮傛璇诲彇 FLV 鏍囩
    void async_read_tag();
    
    /// @brief 澶勭悊 FLV 鏍囩
    /// @param tag_data FLV 鏍囩鏁版嵁
    /// @param tag_size 鏍囩澶у皬
    void handleFlvTag(const uint8_t* data, size_t size);
    
    /// @brief 瑙ｆ瀽 FLV 鏍囩澶?
    /// @param data 鏍囩澶存暟鎹?
    /// @param size 鏁版嵁澶у皬
    /// @return 鏍囩绫诲瀷锛?=鏃犳晥锛?
    int parseFlvTagHeader(const uint8_t* data, size_t size);
    
    /// @brief 浠庤棰戞爣绛句腑鎻愬彇 NALU
    /// @param data 瑙嗛鏍囩鏁版嵁
    /// @param size 鏁版嵁澶у皬
    /// @param pts 鏃堕棿鎴?
    void extractNalu(const uint8_t* data, size_t size, int64_t pts);
    
    /// @brief 閲嶈繛閫昏緫
    void doReconnect();
    
    /// @brief 閲嶇疆鐘舵€?
    void reset();
    
    /// @brief 缂撳瓨鍏抽敭甯ф暟鎹紙鐢ㄤ簬缃戠粶娉㈠姩鎴栬В鐮佸櫒閲嶇疆鏃跺揩閫熸仮澶嶏級
    void cacheKeyframe(int codec_id, const uint8_t* data, size_t size);
    
    /// @brief 鑾峰彇鏈€鍚庣殑 SPS/PPS 鏁版嵁锛堢敤浜庡揩閫熸仮澶嶏級
    const std::vector<uint8_t>& getLastSpsPpsH264() const { return last_sps_pps_h264_; }
    const std::vector<uint8_t>& getLastSpsPpsH265() const { return last_sps_pps_h265_; }
    
    // ==================== 鎴愬憳鍙橀噺 ====================
    /// @brief io_context
    boost::asio::io_context& io_ctx_;
    
    /// @brief TCP socket
    std::unique_ptr<boost::asio::ip::tcp::socket> socket_;
    
    /// @brief FLV 澶寸紦鍐插尯
    std::vector<uint8_t> flv_header_buffer_;
    
    /// @brief HTTP 鍝嶅簲缂撳啿鍖猴紙鐢ㄤ簬璇诲彇 HTTP 鍝嶅簲澶达級
    beast::flat_buffer http_response_buffer_;
    
    /// @brief PreviousTagSize 缂撳啿鍖?
    std::vector<uint8_t> prev_tag_size_buffer_;
    
    /// @brief 鏍囩澶寸紦鍐插尯
    std::vector<uint8_t> tag_header_buffer_;
    
    /// @brief 褰撳墠鏍囩鏁版嵁
    std::vector<uint8_t> current_tag_data_;
    
    /// @brief 鏈€鍚庣殑 SPS/PPS 鏁版嵁锛圚.264锛?
    std::vector<uint8_t> last_sps_pps_h264_;
    
    /// @brief 鏈€鍚庣殑 SPS/PPS 鏁版嵁锛圚.265锛?
    std::vector<uint8_t> last_sps_pps_h265_;
    
    /// @brief 搴忓垪澶村洖璋冨嚱鏁?
    SequenceHeaderCallback seq_callback_;
    
    /// @brief 鏁版嵁鍥炶皟鍑芥暟
    FrameCallback callback_;
    
    /// @brief 鏄惁宸茶鍙?FLV 澶?
    bool has_flv_header_{false};
    
    /// @brief 鏈熸湜鐨勬爣绛惧ぇ灏?
    uint32_t expected_tag_size_{0};
    
    /// @brief 涓绘満鍚?
    std::string host_;
    
    /// @brief 绔彛
    std::string port_;
    
    /// @brief 璺緞
    std::string path_;
    
    /// @brief 閲嶈繛寤惰繜锛堢锛?
    int reconnect_delay_{3};
    
    /// @brief 鏈€澶ч噸杩炴鏁帮紙-1=鏃犻檺锛?
    int max_reconnect_attempts_{-1};
    
    /// @brief 褰撳墠閲嶈繛娆℃暟
    int reconnect_count_{0};
    
    /// @brief 鏄惁宸插仠姝?
    std::atomic<bool> stopped_{false};
    
    /// @brief 鏄惁姝ｅ湪杩愯
    std::atomic<bool> running_{false};
    
    /// @brief 缁熻淇℃伅
    std::atomic<uint64_t> bytes_received_{0};
    std::atomic<uint64_t> tags_processed_{0};
    std::atomic<uint64_t> frames_delivered_{0};
};

