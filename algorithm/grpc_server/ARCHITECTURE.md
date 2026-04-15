# Python 算法模块重构说明

## 📋 概述

本次重构实现了 **gRPC 通信层** 与 **算法实现层** 的完全解耦，采用**控制器模式**管理算法的生命周期和切换。

## 🏗️ 架构设计

### 核心思想

```
┌─────────────────────────────────────┐
│      gRPC Service Layer             │  ← video_service_refactored.py
│  (处理通信、编解码、显示)              │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│   Algorithm Controller              │  ← algorithm_controller.py
│  (管理算法生命周期、切换、统计)        │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│   Base Algorithm (ABC)              │  ← base_algorithm.py
│  (定义算法接口)                       │
└──────────────┬──────────────────────┘
               │
       ┌───────┴────────┐
       ▼                ▼
┌──────────────┐  ┌──────────────┐
│ Mock Algo    │  │ YOLOv5 Algo  │  ← algorithms/
└──────────────┘  └──────────────┘
```

### 职责分离

| 层级 | 文件 | 职责 |
|------|------|------|
| **gRPC 服务层** | `video_service_refactored.py` | - 处理 gRPC 双向流<br>- 视频帧编解码<br>- 调用算法控制器<br>- 显示视频（可选） |
| **算法控制器** | `algorithm_controller.py` | - 注册和管理算法<br>- 运行时切换算法<br>- 统计算法性能<br>- 提供统一调用接口 |
| **算法基类** | `base_algorithm.py` | - 定义算法抽象接口<br>- 定义数据结构（BoundingBox, DetectionResult） |
| **具体算法** | `algorithms/*.py` | - 实现具体算法逻辑<br>- Mock 算法（测试用）<br>- YOLOv5 算法（生产用） |

## 📁 目录结构

```
algorithm/grpc_server/
├── base_algorithm.py              # 算法基类（抽象接口）
├── algorithm_controller.py        # 算法控制器
├── video_service_refactored.py    # gRPC 服务（重构版）
├── video_service.py               # gRPC 服务（旧版，保留兼容）
├── algorithms/                    # 具体算法实现
│   ├── __init__.py
│   ├── mock_algorithm.py         # Mock 算法
│   └── yolov5_algorithm.py       # YOLOv5 算法
├── video_processing_pb2.py        # gRPC 生成的 proto 文件
├── video_processing_pb2_grpc.py   # gRPC 生成的服务文件
├── requirements.txt               # Python 依赖
└── ARCHITECTURE.md                # 本文档
```

## 🚀 使用方法

### 1. 启动服务器（使用 Mock 算法）

```bash
cd algorithm/grpc_server
python video_service_refactored.py --port 50052 --algorithm mock
```

### 2. 启动服务器（使用 YOLOv5 算法）

```bash
python video_service_refactored.py \
    --port 50052 \
    --algorithm yolov5 \
    --model path/to/yolov5s.pt \
    --device cuda
```

### 3. 禁用视频显示（服务器模式）

```bash
python video_service_refactored.py --no-show
```

## 💡 扩展新算法

### 步骤 1: 创建算法类

在 `algorithms/` 目录下创建新文件，例如 `custom_algorithm.py`：

```python
from base_algorithm import BaseAlgorithm, BoundingBox, DetectionResult
import numpy as np


class CustomAlgorithm(BaseAlgorithm):
    def __init__(self, param1=None, param2=None):
        super().__init__(name="CustomAlgorithm")
        self.param1 = param1
        self.param2 = param2
    
    def initialize(self) -> bool:
        """初始化算法"""
        # TODO: 加载模型、初始化资源
        self.is_initialized = True
        return True
    
    def process(self, image: np.ndarray, frame_id: str = "") -> DetectionResult:
        """处理帧"""
        # TODO: 实现检测逻辑
        boxes = []
        
        result = DetectionResult(
            frame_id=frame_id,
            boxes=boxes,
            processing_time_ms=0,
            algorithm="Custom"
        )
        
        return result
    
    def cleanup(self):
        """清理资源"""
        self.is_initialized = False
```

### 步骤 2: 注册算法

在 `video_service_refactored.py` 的 `_register_algorithms()` 方法中添加：

```python
from algorithms.custom_algorithm import CustomAlgorithm

def _register_algorithms(self):
    # ... 现有代码 ...
    
    # 注册自定义算法
    self.controller.register_algorithm(
        name="custom",
        algorithm_class=CustomAlgorithm,
        param1=value1,
        param2=value2
    )
```

### 步骤 3: 启动时选择算法

```bash
python video_service_refactored.py --algorithm custom
```

## 🔄 运行时切换算法

未来可以通过 gRPC 添加切换算法的接口：

```python
# 在 VideoProcessingService 中添加
def SwitchAlgorithm(self, request, context):
    """切换算法"""
    success = self.controller.switch_algorithm(request.algorithm_name)
    return video_processing_pb2.SwitchResponse(success=success)
```

## ✅ 优势

### 1. **解耦清晰**
- gRPC 通信层不关心具体算法实现
- 算法实现不依赖 gRPC
- 可以独立测试和开发

### 2. **易于扩展**
- 新增算法只需继承 `BaseAlgorithm`
- 无需修改 gRPC 服务代码
- 符合开闭原则（Open-Closed Principle）

### 3. **灵活切换**
- 支持运行时切换算法
- 可以动态加载/卸载算法
- 便于 A/B 测试

### 4. **统一管理**
- 算法生命周期由控制器管理
- 统一的性能统计
- 统一的错误处理

### 5. **向后兼容**
- 保留了旧的 `video_service.py`
- 可以逐步迁移
- 不影响现有客户端

## 📊 性能统计

算法控制器自动收集以下统计信息：

```python
stats = controller.get_stats()
# 输出示例：
{
    'frame_count': 1000,
    'total_processing_time_ms': 50000,
    'avg_processing_time_ms': 50,
    'current_algorithm': 'yolov5',
    'available_algorithms': ['mock', 'yolov5']
}
```

## 🔧 C++ 端对应设计

C++ 端也应该采用类似的设计：

```cpp
// modules/alg/include/alg/i_algorithm.h
class IAlgorithm {
public:
    virtual ~IAlgorithm() = default;
    virtual bool Initialize() = 0;
    virtual AlgorithmResult Process(cv::Mat& frame, int channel_id, int64_t pts) = 0;
    virtual void Cleanup() = 0;
    virtual std::string GetName() const = 0;
};

// modules/alg/include/alg/algorithm_controller.h
class AlgorithmController {
public:
    void RegisterAlgorithm(const std::string& name, std::shared_ptr<IAlgorithm> algo);
    bool SwitchAlgorithm(const std::string& name);
    AlgorithmResult ProcessFrame(cv::Mat& frame, int channel_id, int64_t pts);
    // ...
};

// modules/alg/include/alg/grpc/grpc_algorithm_adapter.h
class GrpcAlgorithmAdapter : public IAlgorithmProcessor {
private:
    AlgorithmController controller_;  // 使用控制器
    // ...
};
```

## 📝 下一步

1. ✅ 完成 Python 端重构
2. ⏳ 重构 C++ 端的 `IAlgorithmProcessor`
3. ⏳ 添加运行时切换算法的 gRPC 接口
4. ⏳ 实现 YOLOv5 算法的完整集成
5. ⏳ 添加单元测试

## 🎯 总结

通过引入**算法控制器**和**算法基类**，我们实现了：

- ✅ **关注点分离**：gRPC 通信 vs 算法实现
- ✅ **可扩展性**：轻松添加新算法
- ✅ **可维护性**：清晰的职责划分
- ✅ **可测试性**：独立的算法单元测试
- ✅ **灵活性**：运行时切换算法

这种设计模式不仅适用于当前的视频检测场景，也可以推广到其他需要动态加载算法的场景。
