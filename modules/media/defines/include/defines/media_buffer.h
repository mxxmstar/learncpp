#pragma once
/// @file media_buffer.h
/// C API：不透明缓冲区，提供 create/destroy/data/size 操作。

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/// 不透明缓冲区句柄（内部使用 SimpleBuffer 实现）
typedef struct media_buffer_t media_buffer_t;

/// 创建缓冲区并拷贝 data 中的 size 字节；data 为 NULL 时分配但不填充
media_buffer_t* media_buffer_create(const void* data, size_t size);
/// 销毁缓冲区并释放所有内存
void media_buffer_destroy(media_buffer_t* buf);
/// 返回只读数据指针
const uint8_t*  media_buffer_data(const media_buffer_t* buf);
/// 返回缓冲区有效数据字节数
size_t media_buffer_size(const media_buffer_t* buf);

#ifdef __cplusplus
}
#endif
