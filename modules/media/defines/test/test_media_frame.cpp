// @file test_media_frame.cpp
// 测试 MediaFrame C++ 类和 C API media_frame_t。

#include "defines/media_frame.hpp"
#include "defines/media_frame.h"
#include "defines/media_buffer.h"
#include "defines/i_media_buffer.hpp"
#include "common/log/logmanager.h"
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <memory>

// 测试用的最小 IMediaBuffer 实现
struct TestBuffer : IMediaBuffer {
    uint8_t* data_{nullptr};
    size_t   size_{0};
    explicit TestBuffer(size_t s) : size_(s) {
        if (s > 0) data_ = static_cast<uint8_t*>(std::malloc(s));
    }
    ~TestBuffer() override { std::free(data_); }
    uint8_t* Data() override { return data_; }
    const uint8_t* Data() const override { return data_; }
    size_t Size() const override { return size_; }
};

static void test_cpp_frame_defaults() {
    MediaFrame frame;
    assert(frame.width == 0);
    assert(frame.height == 0);
    assert(frame.pixel_format == PixelFormat::kUnknown);
    assert(frame.pts == 0);
    assert(frame.buffer == nullptr);
    assert(frame.DataY() == nullptr);
    assert(frame.DataUV() == nullptr);
    LOG_MAIN_INFO("[test] MediaFrame default values ... PASS");
}

static void test_cpp_calc_video_size() {
    assert(MediaFrame::CalcVideoSize(1920, 1080, PixelFormat::kNV12) == 1920 * 1080 + 1920 * 1080 / 2);
    assert(MediaFrame::CalcVideoSize(1920, 1080, PixelFormat::kI420) == 1920 * 1080 + 1920 * 1080 / 2);
    assert(MediaFrame::CalcVideoSize(1920, 1080, PixelFormat::kNV21) == 1920 * 1080 + 1920 * 1080 / 2);
    assert(MediaFrame::CalcVideoSize(100, 200, PixelFormat::kRGB24) == 100 * 200 * 3);
    assert(MediaFrame::CalcVideoSize(100, 200, PixelFormat::kBGR24) == 100 * 200 * 3);
    assert(MediaFrame::CalcVideoSize(320, 240, PixelFormat::kGRAY8) == 320 * 240);
    assert(MediaFrame::CalcVideoSize(0, 100, PixelFormat::kNV12) == 0);
    assert(MediaFrame::CalcVideoSize(100, 0, PixelFormat::kNV12) == 0);
    assert(MediaFrame::CalcVideoSize(100, 100, PixelFormat::kUnknown) == 0);
    LOG_MAIN_INFO("[test] CalcVideoSize ... PASS");
}

static void test_cpp_frame_data_y_uv() {
    MediaFrame frame;
    frame.width = 4;
    frame.height = 4;
    frame.pixel_format = PixelFormat::kNV12;
    auto buf = std::make_shared<TestBuffer>(4 * 4 + 4 * 2);
    frame.buffer = buf;
    std::memset(buf->Data(), 0xA0, 4 * 4);
    std::memset(buf->Data() + 4 * 4, 0xB0, 4 * 2);
    assert(frame.DataY() == buf->Data());
    assert(frame.DataUV() == buf->Data() + 4 * 4);
    LOG_MAIN_INFO("[test] MediaFrame DataY/DataUV ... PASS");
}

static void test_c_frame_init_clear() {
    media_frame_t frame;
    media_frame_init(&frame);
    assert(frame.width == 0);
    assert(frame.height == 0);
    assert(frame.pixel_format == PIXEL_FORMAT_UNKNOWN);
    assert(frame.buffer == nullptr);

    frame.width = 320;
    frame.height = 240;
    frame.pixel_format = PIXEL_FORMAT_NV12;
    media_frame_clear(&frame);
    assert(frame.width == 0);
    assert(frame.buffer == nullptr);
    LOG_MAIN_INFO("[test] C frame init/clear ... PASS");
}

static void test_c_frame_with_buffer() {
    const uint8_t src[] = {0x10, 0x20, 0x30};
    media_frame_t frame;
    media_frame_init(&frame);
    frame.buffer = media_buffer_create(src, 3);
    assert(frame.buffer != nullptr);
    assert(media_buffer_size(frame.buffer) == 3);
    media_frame_clear(&frame);
    assert(frame.buffer == nullptr);
    LOG_MAIN_INFO("[test] C frame with buffer ... PASS");
}

static void test_c_calc_video_size() {
    assert(media_frame_calc_video_size(1920, 1080, PIXEL_FORMAT_NV12) == 1920 * 1080 + 1920 * 1080 / 2);
    assert(media_frame_calc_video_size(100, 200, PIXEL_FORMAT_RGB24) == 100 * 200 * 3);
    assert(media_frame_calc_video_size(0, 100, PIXEL_FORMAT_NV12) == 0);
    LOG_MAIN_INFO("[test] C calc_video_size ... PASS");
}

static void test_c_frame_null() {
    media_frame_init(nullptr);
    media_frame_clear(nullptr);
    LOG_MAIN_INFO("[test] C frame null safe ... PASS");
}

int main() {
    LogManager::getInstance().Init();
    LOG_MAIN_INFO("=== media_frame tests ===");
    test_cpp_frame_defaults();
    test_cpp_calc_video_size();
    test_cpp_frame_data_y_uv();
    test_c_frame_init_clear();
    test_c_frame_with_buffer();
    test_c_calc_video_size();
    test_c_frame_null();
    LOG_MAIN_INFO("=== ALL PASS ===");
    LogManager::getInstance().FlushAll();
    return 0;
}
