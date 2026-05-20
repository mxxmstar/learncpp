#pragma once
/// @file i_media_buffer.hpp
/// 媒体数据缓冲区的抽象接口，所有具体缓冲区实现均继承自此类。

#include <cstdint>
#include <cstddef>

/// 媒体缓冲区纯虚基类
class IMediaBuffer {
public:
    virtual ~IMediaBuffer() = default;

    /// 返回可读写的数据指针
    virtual uint8_t* Data() = 0;

    /// 返回只读的数据指针
    virtual const uint8_t* Data() const = 0;

    /// 返回缓冲区有效数据字节数
    virtual size_t Size() const = 0;
};
