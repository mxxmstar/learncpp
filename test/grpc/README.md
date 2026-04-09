# 视频处理 gRPC 快速开始

## 架构概述

```
C++ Client <--双向流--> Python Server (YOLOv5)
     |                        |
  发送视频帧              运行算法
  接收结果                返回结果
```

## 目录结构

```
learncpp/
├── grpc/
│   ├── proto/
│   │   └── video_processing.proto    # Proto 定义
│   ├── include/
│   │   └── video_grpc_client.h       # C++ 客户端头文件
│   ├── src/
│   │   └── video_grpc_client.cpp     # C++ 客户端实现
│   └── generated/                     # 自动生成的代码
│       ├── video_processing.pb.h/cc
│       └── video_processing.grpc.pb.h/cc
│
├── algorithm/grpc_server/
│   ├── video_service.py              # Python 服务端
│   ├── generate_proto.py             # 生成脚本
│   └── requirements.txt              # Python 依赖
│
└── test/grpc/
    ├── test_video_grpc_client.cpp    # C++ 测试
    └── test_video_server.py          # Python 测试
```

## 步骤 1: 安装 Python 依赖

```bash
cd algorithm/grpc_server
pip install -r requirements.txt
```

## 步骤 2: 生成 Python gRPC 代码

```bash
cd algorithm/grpc_server
python generate_proto.py
```

这会生成：
- `video_processing_pb2.py`
- `video_processing_pb2_grpc.py`

## 步骤 3: 启动 Python 服务器

### 选项 A: 使用模拟检测器（推荐先测试）

```bash
cd algorithm/grpc_server
python video_service.py --port 50052
```

### 选项 B: 使用 YOLOv5 模型

```bash
cd algorithm/grpc_server
python video_service.py --port 50052 --model yolov5s.pt --device cuda
```

**输出示例：**
```
[VideoService] Server started on [::]:50052
[VideoService] Model: Mock
[VideoService] Device: cpu
[VideoService] Ready to accept connections...
```

## 步骤 4: 编译 C++ 项目

```bash
cd out/build/x64-Debug
cmake ../..
cmake --build . --config Debug
```

CMake 会自动：
1. 查找 protoc 和 grpc_cpp_plugin
2. 从 `grpc/proto/video_processing.proto` 生成 C++ 代码
3. 编译 `video_grpc_client.cpp`
4. 链接到 `grpc_lib`

## 步骤 5: 运行 C++ 测试

```bash
cd out/build/x64-Debug
.\bin\test_video_grpc_client.exe
```

**选择测试模式：**
```
Select test mode:
1. Detection Stream (Metadata only)
2. Video Process Stream (Processed video)
Enter choice (1 or 2): 1
```

### 模式 1: 检测元数据

- C++ 发送视频帧
- Python 返回检测结果（边界框、类别）
- 低带宽，适合实时分析

**输出示例：**
```
[Callback] Frame: frame_000001, Boxes: 2, Processing time: 45ms
  Box 0: (120, 80) 100x120 conf=0.92
  Box 1: (300, 200) 80x90 conf=0.87
```

### 模式 2: 处理后视频

- C++ 发送原始视频帧
- Python 返回带标注的视频
- 高带宽，适合快速演示

**输出：**
- 显示处理后的视频窗口
- 按 ESC 退出

## 两种工作模式对比

| 特性 | 模式 1（元数据） | 模式 2（视频） |
|------|-----------------|---------------|
| 带宽 | ~100 KB/s | ~5 MB/s |
| 延迟 | 20-50 ms | 50-100 ms |
| C++ 复杂度 | 中（需后处理） | 低（直接显示） |
| 适用场景 | 实时监控、分析 | 快速演示 |

## API 使用示例

### C++ 客户端

```cpp
#include "video_grpc_client.h"

using namespace grpc_module;

// 创建客户端
VideoGrpcClient client("localhost:50052");

// 连接
if (!client.Connect()) {
    std::cerr << "Connection failed" << std::endl;
    return 1;
}

// 模式 1: 检测元数据
client.StartDetectionStream(
    [](const std::string& frame_id, 
       const std::vector<std::map<std::string, float>>& boxes,
       int64_t processing_time_ms) {
        
        std::cout << "Frame: " << frame_id 
                 << ", Boxes: " << boxes.size() << std::endl;
    }
);

// 发送帧
cv::Mat frame = ...; // 你的视频帧
client.SendFrameForDetection(frame);

// 停止
client.StopDetectionStream();
client.Disconnect();
```

### Python 服务端

```python
from grpc_server import start_server

# 启动服务器
start_server(port=50052, model_path=None, device="cpu")
```

## 常见问题

### Q1: 连接失败

**症状：** `Connection timeout`

**解决：**
1. 确认 Python 服务器正在运行
2. 检查端口是否正确（默认 50052）
3. 检查防火墙设置

```bash
# 检查端口
netstat -an | grep 50052
```

### Q2: 找不到生成的代码

**症状：** `fatal error: video_processing.grpc.pb.h: No such file or directory`

**解决：**
重新配置 CMake：

```bash
cd out/build/x64-Debug
cmake ../..
```

CMake 会自动生成代码到 `grpc/generated/`

### Q3: 内存占用高

**解决：**
1. 降低 JPEG 质量（在 C++ 中调整 quality 参数）
2. 减少并发连接数
3. 降低视频分辨率

```cpp
// 降低质量（默认 85）
client.SendFrameForDetection(frame, "", 70);
```

### Q4: 延迟高

**解决：**
1. 使用 CUDA 加速（如果有 GPU）
2. 降低视频分辨率
3. 减少编码质量

```bash
# 使用 CUDA
python video_service.py --model yolov5s.pt --device cuda
```

## 性能调优

### 1. 调整帧率

```cpp
// 30 FPS
std::this_thread::sleep_for(std::chrono::milliseconds(33));

// 15 FPS
std::this_thread::sleep_for(std::chrono::milliseconds(66));
```

### 2. 调整 JPEG 质量

```cpp
// 高质量（大带宽）
client.SendFrameForDetection(frame, "", 95);

// 平衡
client.SendFrameForDetection(frame, "", 85);

// 低带宽
client.SendFrameForDetection(frame, "", 70);
```

### 3. 批量处理

如果需要更低延迟，可以考虑批量发送多帧（需要修改 proto）。

## 下一步

- [ ] 集成到视频流水线
- [ ] 添加更多算法支持
- [ ] 性能基准测试
- [ ] 添加认证和加密
- [ ] 支持批量处理

## 参考

- [完整架构文档](../../algorithm/VIDEO_GRPC_ARCHITECTURE.md)
- [Python 服务端文档](../../algorithm/grpc_server/README.md)
- [gRPC 官方文档](https://grpc.io/docs/)
