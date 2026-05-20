#pragma once
/// @file simple_buffer.hpp
/// 基于 malloc/free 的最简 IMediaBuffer 实现，适用于非 FFmpeg 场景。

#include <cstdlib>
#include <cstring>
#include "i_media_buffer.hpp"

/// 使用 malloc 分配的简单连续缓冲区
class SimpleBuffer : public IMediaBuffer {
public:
    /// 分配 size 字节的缓冲区，size=0 时 data_ 为 nullptr
    explicit SimpleBuffer(size_t size) : size_(size) {
        if (size > 0) data_ = static_cast<uint8_t*>(std::malloc(size));
    }
    ~SimpleBuffer() override { std::free(data_); }
    uint8_t* Data() override { return data_; }
    const uint8_t* Data() const override { return data_; }
    size_t Size() const override { return size_; }
private:
    uint8_t* data_{nullptr};  ///< 缓冲区起始地址
    size_t   size_{0};        ///< 缓冲区容量
};
