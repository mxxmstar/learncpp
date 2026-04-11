#pragma once

#include "decoder/i_decoder.h"
#include <memory>
#include <atomic>
#include <string>

// FFmpeg 鍓嶅悜澹版槑
struct AVCodecContext;
struct AVFrame;
struct AVPacket;

/// @brief FFmpeg 瑙嗛瑙ｇ爜鍣紙绾?FFmpeg锛屼笉渚濊禆 OpenCV锛?
/// 灏?H.264/H.265 NALU 鏁版嵁瑙ｇ爜涓?VideoFrame
class FfmpegDecoder : public IDecoder {
public:
    /// @brief 鏋勯€犲嚱鏁?
    FfmpegDecoder();
    
    /// @brief 鏋愭瀯鍑芥暟
    ~FfmpegDecoder() override;
    
    /// @brief 鎵撳紑瑙ｇ爜鍣?
    /// @param extradata 棰濆鏁版嵁锛圫PS/PPS 绛夛級
    /// @param extradata_size 棰濆鏁版嵁澶у皬
    /// @param codec_id 缂栬В鐮佸櫒 ID锛圓V_CODEC_ID_H264=27, AV_CODEC_ID_HEVC=173锛?
    /// @return true 鎴愬姛锛宖alse 澶辫触
    bool Open(const uint8_t* extradata, int extradata_size, int codec_id) override;
    
    /// @brief 瑙ｇ爜鏁版嵁鍖?
    /// @param packet NALU 鏁版嵁鍖?
    /// @param size 鏁版嵁鍖呭ぇ灏?
    /// @param pts 鏄剧ず鏃堕棿鎴?
    /// @param cb 甯у洖璋冨嚱鏁?
    void Decode(const uint8_t* packet, int size, int64_t pts, FrameCallback cb) override;
    
    /// @brief 鍏抽棴瑙ｇ爜鍣?
    void Close() override;
    
    /// @brief 鏄惁宸叉墦寮€
    bool IsOpened() const { return opened_; }
    
    /// @brief 鑾峰彇缂栬В鐮佸櫒鍚嶇О
    std::string GetCodecName() const { return codec_name_; }
    
    /// @brief 璁剧疆瑙ｇ爜绾跨▼鏁?
    void SetThreadCount(int count) { thread_count_ = count; }
    
    /// @brief 鑾峰彇缁熻淇℃伅
    uint64_t GetPacketsDecoded() const { return packets_decoded_.load(); }
    uint64_t GetFramesDecoded() const { return frames_decoded_.load(); }
    
private:
    /// @brief 灏?AVFrame 杞崲涓?VideoFrame锛堟繁鎷疯礉鏁版嵁锛?
    /// @param av_frame FFmpeg 甯?
    /// @return 閫氱敤瑙嗛甯?
    VideoFrame convertToVideoFrame(AVFrame* av_frame);
    
    /// @brief 澶勭悊瑙ｇ爜鍚庣殑甯?
    /// @param av_frame 瑙ｇ爜鍚庣殑甯?
    /// @param pts 鏃堕棿鎴?
    /// @param cb 鍥炶皟鍑芥暟
    void processDecodedFrame(AVFrame* av_frame, int64_t pts, FrameCallback cb);
    
    // FFmpeg 涓婁笅鏂?
    AVCodecContext* codec_ctx_ = nullptr;
    AVFrame* frame_ = nullptr;
    AVPacket* pkt_ = nullptr;
    
    // 缂栬В鐮佸櫒淇℃伅
    std::string codec_name_;
    int codec_id_ = 0;
    
    // 閰嶇疆
    int thread_count_;  // 瑙ｇ爜绾跨▼鏁?
    
    // 鐘舵€?
    std::atomic<bool> opened_{false};
    
    // 缁熻淇℃伅
    std::atomic<uint64_t> packets_decoded_{0};
    std::atomic<uint64_t> frames_decoded_{0};
};

