# Algorithm 模块重构说明

## 📋 概述

本次重构将 **算法实现** 与 **gRPC 通信** 完全分离，采用清晰的模块化设计。

## 🏗️ 目录结构

```
algorithm/
├── algorithms/                    # ✅ 所有算法实现（与 gRPC 无关）
│   ├── __init__.py
│   ├── base_algorithm.py         # 算法基类（抽象接口）
│   ├── mock/                     # Mock 算法（测试用）
│   │   ├── __init__.py
│   │   └── mock_algorithm.py
│   └── yolov5/                   # YOLOv5 算法（生产用）
│       ├── __init__.py
│       ├── detector.py           # YOLOv5 检测器（OpenVINO）
│       ├── preprocessor.py       # Letterbox 预处理器
│       ├── postprocessor.py      # NMS 后处理器
│       ├── utils.py              # 工具函数
│       └── yolov5_algorithm.py   # 算法适配器（继承 BaseAlgorithm）
│
├── grpc_server/                  # ✅ gRPC 服务端（只负责通信）
│   ├── __init__.py
│   ├── algorithm_controller.py   # 算法控制器
│   ├── video_service_refactored.py  # gRPC 服务（重构版）
│   ├── video_service.py          # gRPC 服务（旧版，保留兼容）
│   ├── video_processing_pb2.py   # gRPC 生成的 proto
│   ├── video_processing_pb2_grpc.py
│   ├── server.py                 # 启动脚本（可选）
│   ├── requirements.txt
│   └── ARCHITECTURE.md           # 架构文档
│
├── grpc_client/                  # ✅ gRPC 客户端（保持不变）
│   └── ...
│
├── proto/                        # ✅ Proto 文件
│   └── video_processing.proto
│
└── README.md                     # 本文档
```

## 🎯 设计原则

### 1. **关注点分离**

| 模块 | 职责 | 位置 |
|------|------|------|
| **算法实现** | - 图像处理<br>- 模型推理<br>- 后处理 | `algorithms/` |
| **算法控制器** | - 管理算法生命周期<br>- 切换算法<br>- 性能统计 | `grpc_server/algorithm_controller.py` |
| **gRPC 服务** | - 网络通信<br>- 帧编解码<br>- 调用控制器 | `grpc_server/video_service_*.py` |

### 2. **依赖关系**

```
gRPC Service → Algorithm Controller → Base Algorithm → Specific Algorithm
     ↓                ↓                       ↓               ↓
  通信层           管理层                 接口层          实现层
```

- ✅ gRPC 服务依赖算法控制器
- ✅ 算法控制器依赖算法基类
- ✅ 具体算法继承算法基类
- ❌ 算法实现不依赖 gRPC

### 3. **可扩展性**

添加新算法只需 3 步：

1. 在 `algorithms/` 下创建新目录
2. 继承 `BaseAlgorithm` 实现算法
3. 在控制器中注册

**无需修改 gRPC 代码！**

## 🚀 使用方法

### 1. 启动服务器（Mock 算法）

```bash
cd algorithm/grpc_server
python video_service_refactored.py --port 50052 --algorithm mock
```

### 2. 启动服务器（YOLOv5 算法）

```bash
python video_service_refactored.py \
    --port 50052 \
    --algorithm yolov5 \
    --model path/to/yolov5.xml \
    --device CPU
```

### 3. 独立测试算法（不启动 gRPC）

```python
import sys
sys.path.insert(0, 'algorithm')

from algorithms import YOLOv5Algorithm
import cv2

# 创建算法实例
algo = YOLOv5Algorithm(
    model_path="path/to/yolov5.xml",
    device="CPU"
)

# 初始化
if algo.initialize():
    # 加载图像
    image = cv2.imread("test.jpg")
    
    # 处理
    result = algo.process(image, frame_id="test_001")
    
    # 输出结果
    print(f"Detected {len(result.boxes)} objects")
    for box in result.boxes:
        print(f"  {box.class_name}: {box.confidence:.2f}")
    
    # 清理
    algo.cleanup()
```

## 💡 扩展新算法示例

### 步骤 1: 创建算法目录

```bash
mkdir algorithm/algorithms/custom_algo
```

### 步骤 2: 实现算法

```python
# algorithm/algorithms/custom_algo/custom_algorithm.py

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from base_algorithm import BaseAlgorithm, BoundingBox, DetectionResult
import numpy as np


class CustomAlgorithm(BaseAlgorithm):
    def __init__(self, param1=None):
        super().__init__(name="CustomAlgorithm")
        self.param1 = param1
    
    def initialize(self) -> bool:
        """初始化"""
        # TODO: 加载模型
        self.is_initialized = True
        return True
    
    def process(self, image: np.ndarray, frame_id: str = "") -> DetectionResult:
        """处理帧"""
        # TODO: 实现检测逻辑
        boxes = []
        
        return DetectionResult(
            frame_id=frame_id,
            boxes=boxes,
            processing_time_ms=0,
            algorithm="Custom"
        )
    
    def cleanup(self):
        """清理"""
        self.is_initialized = False
```

### 步骤 3: 创建 __init__.py

```python
# algorithm/algorithms/custom_algo/__init__.py

from algorithms.custom_algo.custom_algorithm import CustomAlgorithm

__all__ = ['CustomAlgorithm']
```

### 步骤 4: 注册算法

在 `grpc_server/video_service_refactored.py` 的 `_register_algorithms()` 中添加：

```python
from algorithms.custom_algo import CustomAlgorithm

def _register_algorithms(self):
    # ... 现有代码 ...
    
    self.controller.register_algorithm(
        name="custom",
        algorithm_class=CustomAlgorithm,
        param1=value
    )
```

### 步骤 5: 启动

```bash
python video_service_refactored.py --algorithm custom
```

## 📊 YOLOv5 算法集成

### 已有的 YOLOv5 实现

位于 `algorithm/yolov5/` 目录，包含：

- ✅ `detector.py` - 同步/异步检测器
- ✅ `preprocessor.py` - Letterbox 预处理
- ✅ `postprocessor.py` - NMS 后处理
- ✅ `utils.py` - 工具函数
- ✅ `demo.py` - 使用示例

### 集成方式

已创建适配器 `algorithms/yolov5/yolov5_algorithm.py`：

```python
class YOLOv5Algorithm(BaseAlgorithm):
    def __init__(self, model_path, device="CPU", async_mode=True):
        # 使用现有的 YOLOv5AsyncDetector 或 YOLOv5SyncDetector
        if async_mode:
            self.detector = YOLOv5AsyncDetector(model_path, device)
        else:
            self.detector = YOLOv5SyncDetector(model_path, device)
    
    def process(self, image, frame_id=""):
        # 调用现有检测器
        detections = self.detector.detect_async(image)
        
        # 转换为标准格式
        boxes = [...]
        return DetectionResult(...)
```

### 优势

- ✅ 复用现有的高性能实现
- ✅ 支持异步推理（多并发）
- ✅ 支持 CPU/GPU
- ✅ 符合新的架构规范

## 🔧 C++ 端对应设计

C++ 端也应该采用类似的设计：

```
modules/alg/
├── include/alg/
│   ├── i_algorithm.h              # 纯算法接口
│   ├── algorithm_controller.h     # 算法控制器
│   └── grpc/
│       └── grpc_algorithm_adapter.h  # gRPC 适配器
├── src/
│   ├── algorithms/                # 具体算法
│   │   ├── null_algorithm.cpp
│   │   ├── motion_detection.cpp
│   │   └── yolov5_algorithm.cpp
│   └── grpc/
│       └── grpc_algorithm_adapter.cpp
```

## ✅ 重构优势

### 之前的问题
- ❌ 算法和 gRPC 混在一起
- ❌ 难以添加新算法
- ❌ 难以单元测试
- ❌ 代码复用困难

### 现在的优势
- ✅ **清晰分层**：算法 vs 通信
- ✅ **易于扩展**：添加算法不影响 gRPC
- ✅ **独立测试**：算法可以单独测试
- ✅ **代码复用**：算法可用于其他场景（非 gRPC）
- ✅ **灵活切换**：运行时切换算法

## 📝 下一步

1. ✅ 完成 Python 端重构
2. ⏳ 重构 C++ 端的 `IAlgorithmProcessor`
3. ⏳ 添加完整的 YOLOv5 集成测试
4. ⏳ 添加运行时切换算法的 gRPC 接口
5. ⏳ 编写单元测试

## 🎯 总结

通过重新组织目录结构，我们实现了：

- ✅ **算法独立性** - `algorithms/` 完全不依赖 gRPC
- ✅ **清晰的职责** - 每层只做自己的事
- ✅ **易于维护** - 修改算法不影响通信
- ✅ **易于扩展** - 添加新算法非常简单
- ✅ **向后兼容** - 保留了旧的 `algorithm/yolov5/` 目录

这种设计不仅适用于当前的视频检测场景，也可以推广到任何需要动态加载算法的场景。
