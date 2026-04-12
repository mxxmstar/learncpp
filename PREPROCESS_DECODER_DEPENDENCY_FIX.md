# Preprocess 模块依赖修复

## 问题

编译 preprocess 模块时出现错误：

```
fatal error C1083: 无法打开包括文件: "decoder/i_decoder.h": No such file or directory
未定义标识符 "VideoFrame"
```

## 原因

`opencv_format_converter.h` 包含了 `decoder/i_decoder.h`，使用了 `VideoFrame` 结构：

```cpp
#include "decoder/i_decoder.h"  // ← 需要 decoder 模块

/// @brief OpenCV 格式转换器（将 VideoFrame 转换为 cv::Mat）
class OpenCVFormatConverter : public IFormatConverter {
    // 使用 VideoFrame 结构
};
```

但 preprocess 模块的 CMakeLists.txt 中没有链接 `decoder_lib`。

## 解决方案

在 `modules/preprocess/CMakeLists.txt` 中添加 `decoder_lib` 依赖：

```cmake
target_link_libraries(preprocess_lib
    PUBLIC
        ${OpenCV_LIBS}
        decoder_lib  # ← 添加这一行
        log_lib
)
```

## 修改的文件

- ✅ `modules/preprocess/CMakeLists.txt` - 添加 decoder_lib 依赖
- ✅ `CAMERA_DECODER_PREPROCESS_MIGRATION.md` - 更新文档

## 依赖关系

```
preprocess_lib
├── OpenCV Libraries
├── decoder_lib      # ← 新增依赖
│   └── FFmpeg Libraries
└── log_lib
```

## 验证

重新配置并编译：

```powershell
# 在 Visual Studio 中
Project → Delete Cache and Reconfigure
Build → Build All
```

应该能成功编译 preprocess_lib。

## 注意事项

由于 preprocess 依赖 decoder，CMakeLists.txt 中的模块顺序必须保证：

```cmake
add_subdirectory(modules/decoder)     # 先添加 decoder
add_subdirectory(modules/preprocess)  # 再添加 preprocess
```

当前顺序已正确。
