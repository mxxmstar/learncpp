#pragma once

#include "puller/zlm/zlm_httpflv_puller.h"
#include "decoder/ffmpeg_decoder.h"
#include "decoder/i_decoder.h"
#include "videopipeline/frame_queue.h"
#include "videopipeline/pipeline_config.h"
#include "videopipeline/i_algorithm_backend.h"
#include "videopipeline/algorithm_backend_factory.h"
#include "osd/yuv/yuv_osd_renderer.h"
#include "pusher/i_pusher.h"
#include <boost/asio.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#include <thread>
#include <atomic>
#include <memory>
#include <mutex>

using VideoFramePtr = std::shared_ptr<VideoFrame>;

class VideoPipeline {
public:
    explicit VideoPipeline(boost::asio::io_context& io_ctx, const PipelineConfig& config);
    ~VideoPipeline();

    bool start();
    void stop();
    bool isRunning() const { return running_; }

    int getChannelId() const { return config_.channel_id; }

    uint64_t getFramesReceived() const { return frames_received_.load(); }
    uint64_t getFramesDecoded() const { return frames_decoded_.load(); }
    uint64_t getFramesProcessed() const { return frames_processed_.load(); }

    using ResultCallback = std::function<void(int channel_id, const DetectionResult& result)>;
    void setResultCallback(ResultCallback callback) {
        if (algorithm_backend_) {
            algorithm_backend_->setResultCallback(callback);
        }
    }

    using FrameOutputCallback = std::function<void(int channel_id, cv::Mat&& frame, int64_t pts)>;
    void setFrameOutputCallback(FrameOutputCallback) {}

    void SetPusher(std::unique_ptr<IPusher> pusher) { pusher_ = std::move(pusher); }
    void SetPushTimeout(int timeout_ms) { push_timeout_ms_ = timeout_ms; }

private:
    void onSequenceHeaderReceived(int codec_id, const uint8_t* data, int size);
    void onNaluReceived(const uint8_t* data, int size, int64_t pts);
    void onFrameDecoded(VideoFrame&& frame);
    void decoderLoop();
    bool initializeAlgorithmBackend();

    void inferenceLoop();
    void pushLoop();

    PipelineConfig config_;
    boost::asio::io_context& io_ctx_;

    std::unique_ptr<ZlmHttpFlvPuller> puller_;
    std::unique_ptr<FfmpegDecoder> decoder_;
    std::unique_ptr<IAlgorithmBackend> algorithm_backend_;
    std::unique_ptr<YuvOsdRenderer> osd_renderer_;
    std::unique_ptr<IPusher> pusher_;

    boost::lockfree::spsc_queue<VideoFramePtr> inference_queue_{64};
    boost::lockfree::spsc_queue<VideoFramePtr> push_queue_{64};

    std::thread decoder_thread_;
    std::thread inference_thread_;
    std::thread push_thread_;
    std::atomic<bool> dispatcher_running_{false};

    std::shared_ptr<RawPacketQueue> raw_queue_;
    std::vector<uint8_t> sps_pps_data_;
    std::vector<uint8_t> sps_pps_h265_data_;

    std::atomic<bool> running_{false};
    std::atomic<bool> decoder_initialized_{false};

    std::atomic<uint64_t> frames_received_{0};
    std::atomic<uint64_t> frames_decoded_{0};
    std::atomic<uint64_t> frames_processed_{0};

    DetectionResult last_result_;
    std::mutex result_mutex_;
    int push_timeout_ms_ = 30;
};