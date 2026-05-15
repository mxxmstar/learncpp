#include "videopipeline/video_pipeline.h"
#include "common/log/logmanager.h"

#include <cstring>
#include <chrono>

VideoPipeline::VideoPipeline(boost::asio::io_context& io_ctx, const PipelineConfig& config)
    : config_(config)
    , io_ctx_(io_ctx)
{
    puller_ = std::make_unique<ZlmHttpFlvPuller>(io_ctx_);
    puller_->SetReconnectParams(config_.puller.reconnect_delay, config_.puller.max_reconnect_attempts);

    decoder_ = std::make_unique<FfmpegDecoder>();
    decoder_->SetThreadCount(config_.decoder.decoder_threads);

    osd_renderer_ = std::make_unique<YuvOsdRenderer>(config_.osd.config);
    osd_renderer_->SetChannelId(config_.channel_id);

    if (!initializeAlgorithmBackend()) {
        LOG_MAIN_WARN_AT("No algorithm backend initialized, running without AI");
    }

    raw_queue_ = std::make_shared<RawPacketQueue>(config_.decoder.raw_queue_size);

    LOG_MAIN_INFO_AT("VideoPipeline created: channel={}, url={}, algorithm={}",
        config_.channel_id, config_.puller.stream_url,
        config_.algorithm.getActiveAlgorithm());
}

VideoPipeline::~VideoPipeline() {
    stop();
}

bool VideoPipeline::start() {
    if (running_) {
        LOG_MAIN_WARN_AT("Pipeline already running");
        return false;
    }
    running_ = true;

    try {
        bool success = puller_->Start(config_.puller.stream_url,
            [this](int codec_id, const uint8_t* data, int size) {
                onSequenceHeaderReceived(codec_id, data, size);
            },
            [this](const uint8_t* data, int size, int64_t pts) {
                onNaluReceived(data, size, pts);
            });

        if (!success) {
            LOG_MAIN_ERROR_AT("Failed to start puller");
            running_ = false;
            return false;
        }

        dispatcher_running_ = true;

        decoder_thread_ = std::thread([this]() { decoderLoop(); });
        inference_thread_ = std::thread([this]() { inferenceLoop(); });
        push_thread_ = std::thread([this]() { pushLoop(); });

        LOG_MAIN_INFO_AT("VideoPipeline started: channel={}", config_.channel_id);
        return true;
    }
    catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("Failed to start pipeline: {}", e.what());
        stop();
        return false;
    }
}

void VideoPipeline::stop() {
    if (!running_.exchange(false)) return;

    LOG_MAIN_INFO_AT("VideoPipeline stopping channel={}: decoded={}, pushed={}",
        config_.channel_id, frames_decoded_.load(), frames_processed_.load());

    if (algorithm_backend_) {
        algorithm_backend_->stop();
    }

    puller_->Stop();
    dispatcher_running_ = false;

    if (decoder_thread_.joinable()) decoder_thread_.join();
    if (inference_thread_.joinable()) inference_thread_.join();
    if (push_thread_.joinable()) push_thread_.join();

    decoder_->Close();

    LOG_MAIN_INFO_AT("VideoPipeline stopped: channel={}", config_.channel_id);
}

void VideoPipeline::onSequenceHeaderReceived(int codec_id, const uint8_t* data, int size) {
    LOG_MAIN_INFO_AT("Received sequence header: codec={}, size={}", codec_id, size);

    if (codec_id == 7) {
        sps_pps_data_.assign(data, data + size);
        if (!decoder_initialized_ && sps_pps_data_.size() > 10) {
            if (decoder_->Open(sps_pps_data_.data(), sps_pps_data_.size(), 27)) {
                decoder_initialized_ = true;
                LOG_MAIN_INFO_AT("H.264 Decoder initialized with {} bytes", sps_pps_data_.size());
            }
        }
    }
    else if (codec_id == 12) {
        sps_pps_h265_data_.assign(data, data + size);
        if (!decoder_initialized_ && sps_pps_h265_data_.size() > 10) {
            if (decoder_->Open(sps_pps_h265_data_.data(), sps_pps_h265_data_.size(), 173)) {
                decoder_initialized_ = true;
                LOG_MAIN_INFO_AT("H.265 Decoder initialized with {} bytes", sps_pps_h265_data_.size());
            }
        }
    }
}

void VideoPipeline::onNaluReceived(const uint8_t* data, int size, int64_t pts) {
    frames_received_++;
    RawPacketData packet(config_.channel_id, pts, data, size);
    if (!raw_queue_->push(std::move(packet))) {
        static int dropped = 0;
        if (++dropped % 100 == 0) {
            LOG_MAIN_WARN_AT("Raw queue full, dropped {} frames", dropped);
        }
    }
}

void VideoPipeline::onFrameDecoded(VideoFrame&& frame) {
    frames_decoded_++;

    auto shared = std::make_shared<VideoFrame>(std::move(frame));

    while (!inference_queue_.push(shared)) {
        VideoFramePtr drop;
        inference_queue_.pop(drop);
    }
    while (!push_queue_.push(shared)) {
        VideoFramePtr drop;
        push_queue_.pop(drop);
    }
}

void VideoPipeline::decoderLoop() {
    LOG_MAIN_INFO_AT("Decoder thread started: channel={}", config_.channel_id);

    while (running_) {
        auto packet_opt = raw_queue_->pop(std::chrono::milliseconds(100));
        if (!packet_opt) continue;

        auto& packet = *packet_opt;

        if (!decoder_initialized_) {
            LOG_MAIN_DEBUG_AT("Decoder not initialized yet, skipping frame");
            continue;
        }

        decoder_->Decode(packet.data.data(), static_cast<int>(packet.data.size()), packet.pts,
            [this](VideoFrame&& frame) {
                onFrameDecoded(std::move(frame));
            });
    }

    LOG_MAIN_INFO_AT("Decoder thread stopped: channel={}", config_.channel_id);
}

bool VideoPipeline::initializeAlgorithmBackend() {
    algorithm_backend_ = AlgorithmBackendFactory::create(config_.algorithm);
    if (!algorithm_backend_) {
        LOG_MAIN_WARN_AT("No algorithm backend created");
        return false;
    }

    if (!algorithm_backend_->initialize(config_.algorithm)) {
        LOG_MAIN_ERROR_AT("Algorithm backend init failed: {}", algorithm_backend_->getBackendType());
        return false;
    }

    algorithm_backend_->setResultCallback([this](int channel_id, const DetectionResult& result) {
        {
            std::lock_guard<std::mutex> lock(result_mutex_);
            last_result_ = result;
        }
        frames_processed_++;
    });

    LOG_MAIN_INFO_AT("Algorithm backend initialized: {}", algorithm_backend_->getBackendType());
    return true;
}

void VideoPipeline::inferenceLoop() {
    LOG_MAIN_INFO_AT("Inference thread started: channel={}", config_.channel_id);

    while (dispatcher_running_) {
        VideoFramePtr frame;
        if (!inference_queue_.pop(frame)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        if (osd_renderer_ && config_.osd.enabled) {
            osd_renderer_->DrawFilledRect(
                frame->data[0], frame->data[1], frame->data[2],
                frame->width, frame->height,
                frame->linesize[0], frame->linesize[1],
                10, 10, 200, 80, 0.5f);

            {
                std::lock_guard<std::mutex> lock(result_mutex_);
                std::vector<OsdRect> rects;
                for (const auto& box : last_result_.boxes) {
                    rects.push_back({
                        static_cast<int>(box.x),
                        static_cast<int>(box.y),
                        static_cast<int>(box.x + box.width),
                        static_cast<int>(box.y + box.height),
                        2,
                        OsdColors::Green
                    });
                }
                osd_renderer_->DrawRects(
                    frame->data[0], frame->data[1], frame->data[2],
                    frame->width, frame->height,
                    frame->linesize[0], frame->linesize[1], rects);
            }

            auto panel = osd_renderer_->BuildInfoPanel(
                frame->pts, 0.0f, config_.channel_id,
                frame->width, frame->height);
            osd_renderer_->DrawTexts(
                frame->data[0], frame->data[1], frame->data[2],
                frame->width, frame->height,
                frame->linesize[0], frame->linesize[1], panel.lines);

            frame->osd_done = true;
        }

        if (algorithm_backend_ && algorithm_backend_->isInitialized()) {
            algorithm_backend_->processFrame(static_cast<const VideoFrame&>(*frame));
        }
    }

    LOG_MAIN_INFO_AT("Inference thread stopped: channel={}", config_.channel_id);
}

void VideoPipeline::pushLoop() {
    LOG_MAIN_INFO_AT("Push thread started: channel={}", config_.channel_id);

    auto timeout = std::chrono::milliseconds(push_timeout_ms_);

    while (dispatcher_running_) {
        VideoFramePtr frame;
        if (!push_queue_.pop(frame)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        auto deadline = std::chrono::steady_clock::now() + timeout;
        while (!frame->osd_done && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::yield();
        }

        if (pusher_ && pusher_->IsRunning()) {
            pusher_->PushYuvFrame(
                frame->data[0], frame->data[1], frame->data[2],
                frame->width, frame->height,
                frame->linesize[0], frame->linesize[1],
                frame->pts);
        }
    }

    LOG_MAIN_INFO_AT("Push thread stopped: channel={}", config_.channel_id);
}