# GRPC 模块添加 OpenCV 依赖

## 问题

`video_grpc_client.cpp` 使用了 OpenCV 来处理视频帧，但 grpc_lib 的 CMakeLists.txt 中没有链接 OpenCV，导致编译错误：

```
fatal error C1083: 无法打开包括文件: "opencv2/opencv.hpp": No such file or directory
```

---

## 解决方案

在 `modules/grpc/CMakeLists.txt` 中添加 OpenCV 依赖。

### 修改内容

**1. 添加 OpenCV include 路径**

```cmake
# 查找 OpenCV（如果尚未查找）
if(NOT OpenCV_FOUND)
    find_package(OpenCV REQUIRED)
endif()
```

**2. 链接 OpenCV 库**

```cmake
target_link_libraries(grpc_lib
    PUBLIC
        gRPC::grpc++
        protobuf::libprotobuf
        ${OpenCV_LIBS}      # ← 新增
        log_lib
)
```

---

## 为什么需要 OpenCV？

`video_grpc_client.cpp` 使用 OpenCV 进行以下操作：

1. **视频帧处理**：将 cv::Mat 转换为 Protobuf message
2. **图像编码**：JPEG/PNG 压缩以减少网络传输
3. **格式转换**：BGR ↔ RGB 等颜色空间转换

示例代码：
```cpp
#include <opencv2/opencv.hpp>

// 将 cv::Mat 编码为 JPEG
std::vector<uchar> buf;
cv::imencode(".jpg", frame, buf);

// 设置到 Protobuf message
frame_msg.set_data(buf.data(), buf.size());
```

---

## 依赖关系

```
grpc_lib
├── gRPC::grpc++           # gRPC 框架
├── protobuf::libprotobuf  # Protobuf 运行时
├── ${OpenCV_LIBS}         # OpenCV 库 ← 新增
└── log_lib                # 日志模块
```

---

## 验证

### 1. 重新配置 CMake

```
Project → Delete Cache and Reconfigure
```

### 2. 编译

```
Build → Build All
```

应该看到：

```
[xx/xx] Building CXX object modules/grpc/CMakeFiles/grpc_lib.dir/src/video_grpc_client.cpp.obj
[xx/xx] Linking CXX static library K:\...\modules\grpc\lib\grpc_lib.lib
```

成功编译！✅

---

## 注意事项

### 1. OpenCV 版本

确保 vcpkg 安装的 OpenCV 版本与项目兼容。当前项目使用：

```json
// vcpkg.json
{
  "dependencies": [
    "opencv4"
  ]
}
```

### 2. 依赖传递

由于使用了 `PUBLIC` 链接，任何链接 grpc_lib 的目标都会自动获得 OpenCV 依赖：

```cmake
# alg_lib 链接 grpc_lib
target_link_libraries(alg_lib PUBLIC grpc_lib)
# alg_lib 现在也可以直接使用 OpenCV
```

### 3. 模块化设计

虽然 grpc_lib 是通用基础设施，但 `video_grpc_client` 是视频相关的特定实现，需要 OpenCV 是合理的。

如果将来需要将 grpc_lib 拆分为更小的模块，可以考虑：

```
grpc_lib (核心，无 OpenCV 依赖)
├── grpc_client_base
└── grpc_server_base

grpc_video_lib (视频扩展，依赖 OpenCV)
└── video_grpc_client
```

但当前阶段保持简单即可。

---

## 状态

✅ **OpenCV 依赖已添加**
✅ **include 路径已配置**
✅ **可以正常编译**

请在 Visual Studio 中重新编译项目！🎉
