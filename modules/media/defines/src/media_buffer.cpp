// @file media_buffer.cpp
// C API media_buffer_t 实现：内部使用 SimpleBuffer 存储数据。

#include "defines/media_buffer.h"
#include "defines/simple_buffer.hpp"
#include <cstring>
#include <new>

// opaque 结构体内部定义，对外部隐藏
struct media_buffer_t {
    void* impl{nullptr};  // 指向 SimpleBuffer 实例
};

media_buffer_t* media_buffer_create(const void* data, size_t size) {
    auto* buf = new (std::nothrow) media_buffer_t;
    if (!buf) return nullptr;
    auto* sb = new (std::nothrow) SimpleBuffer(size);
    // 确保分配成功（size=0 时 data_ 可为 nullptr）
    if (!sb || (size > 0 && !sb->Data())) {
        delete sb;
        delete buf;
        return nullptr;
    }
    if (data && size > 0) {
        std::memcpy(sb->Data(), data, size);
    }
    buf->impl = sb;
    return buf;
}

void media_buffer_destroy(media_buffer_t* buf) {
    if (!buf) return;
    delete static_cast<SimpleBuffer*>(buf->impl);
    delete buf;
}

const uint8_t* media_buffer_data(const media_buffer_t* buf) {
    if (!buf || !buf->impl) return nullptr;
    return static_cast<SimpleBuffer*>(buf->impl)->Data();
}

size_t media_buffer_size(const media_buffer_t* buf) {
    if (!buf || !buf->impl) return 0;
    return static_cast<SimpleBuffer*>(buf->impl)->Size();
}
