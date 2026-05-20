// @file test_media_buffer_simple.cpp
// 测试 SimpleBuffer 和 C API media_buffer_t。

#include "defines/simple_buffer.hpp"
#include "defines/media_buffer.h"
#include "common/log/logmanager.h"
#include <cassert>
#include <cstring>

static void test_simple_buffer_create_empty() {
    SimpleBuffer buf(0);
    assert(buf.Data() == nullptr);
    assert(buf.Size() == 0);
    LOG_MAIN_INFO("[test] SimpleBuffer empty create ... PASS");
}

static void test_simple_buffer_create_with_data() {
    SimpleBuffer buf(16);
    assert(buf.Data() != nullptr);
    assert(buf.Size() == 16);
    std::memset(buf.Data(), 0xAB, 16);
    for (size_t i = 0; i < 16; ++i)
        assert(buf.Data()[i] == 0xAB);
    LOG_MAIN_INFO("[test] SimpleBuffer with data ... PASS");
}

static void test_c_buffer_create_destroy() {
    media_buffer_t* buf = media_buffer_create(nullptr, 0);
    assert(buf != nullptr);
    assert(media_buffer_data(buf) == nullptr);
    assert(media_buffer_size(buf) == 0);
    media_buffer_destroy(buf);
    LOG_MAIN_INFO("[test] C media_buffer create/destroy ... PASS");
}

static void test_c_buffer_with_data() {
    const uint8_t src[] = {1, 2, 3, 4, 5};
    media_buffer_t* buf = media_buffer_create(src, 5);
    assert(buf != nullptr);
    assert(media_buffer_size(buf) == 5);
    assert(std::memcmp(media_buffer_data(buf), src, 5) == 0);
    media_buffer_destroy(buf);
    LOG_MAIN_INFO("[test] C media_buffer with data ... PASS");
}

static void test_c_buffer_null_destroy() {
    media_buffer_destroy(nullptr);
    LOG_MAIN_INFO("[test] C media_buffer null destroy ... PASS");
}

static void test_c_buffer_null_data() {
    assert(media_buffer_data(nullptr) == nullptr);
    assert(media_buffer_size(nullptr) == 0);
    LOG_MAIN_INFO("[test] C media_buffer null data/size ... PASS");
}

int main() {
    LogManager::getInstance().Init();
    LOG_MAIN_INFO("=== media_buffer_simple tests ===");
    test_simple_buffer_create_empty();
    test_simple_buffer_create_with_data();
    test_c_buffer_create_destroy();
    test_c_buffer_with_data();
    test_c_buffer_null_destroy();
    test_c_buffer_null_data();
    LOG_MAIN_INFO("=== ALL PASS ===");
    LogManager::getInstance().FlushAll();
    return 0;
}