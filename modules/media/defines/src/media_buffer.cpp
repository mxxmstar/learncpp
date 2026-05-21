// @file media_buffer.cpp
// C API media_buffer_t 实现：基于 malloc/free 的连续缓冲区。

#include "defines/media_buffer.h"
#include <cstdlib>
#include <cstring>
#include <new>

struct media_buffer_t {
    uint8_t* data{nullptr};
    size_t   size{0};
};

media_buffer_t* media_buffer_create(const void* data, size_t size) {
    auto* buf = new (std::nothrow) media_buffer_t;
    if (!buf) return nullptr;
    if (size > 0) {
        buf->data = static_cast<uint8_t*>(std::malloc(size));
        if (!buf->data) {
            delete buf;
            return nullptr;
        }
        buf->size = size;
        if (data) {
            std::memcpy(buf->data, data, size);
        }
    }
    return buf;
}

void media_buffer_destroy(media_buffer_t* buf) {
    if (!buf) return;
    std::free(buf->data);
    delete buf;
}

const uint8_t* media_buffer_data(const media_buffer_t* buf) {
    return buf ? buf->data : nullptr;
}

size_t media_buffer_size(const media_buffer_t* buf) {
    return buf ? buf->size : 0;
}
