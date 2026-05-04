# C++ 算法模块重构计划

## 📋 目录

1. [项目背景](#项目背景)
2. [架构设计](#架构设计)
3. [模块划分](#模块划分)
4. [TODO 列表](#todo-列表)
5. [技术选型](#技术选型)
6. [实施步骤](#实施步骤)
7. [风险评估](#风险评估)

---

## 项目背景

### 当前状态

**Python 算法模块**（`algorithm/`）：
- ✅ 基于 YOLOv5 + OpenVINO 实现
- ✅ 支持异步推理（多请求并发）
- ✅ gRPC 通信层已实现
- ❌ Python 性能瓶颈（GIL、解释器开销）
- ❌ 部署复杂（需要 Python 环境、依赖管理）
- ❌ 与 C++ 主程序跨进程通信（序列化开销）

**C++ 算法框架**（`modules/alg/`）：
- ✅ 已有基础接口定义（`IAlgorithm`, `IAlgorithmProcessor`）
- ✅ gRPC 客户端已实现（与 Python 后端通信）
- ⚠️ 缺少原生 C++ 算法实现
- ⚠️ 缺少模型加载和推理引擎

### 重构目标

1. **性能提升**：消除 Python GIL 和跨进程通信开销
2. **简化部署**：单一可执行文件，无需 Python 环境
3. **代码统一**：全部使用 C++20，统一技术栈
4. **保持兼容**：保留 gRPC 接口，支持渐进式迁移

---

## 架构设计

### 整体架构图

```
┌─────────────────────────────────────────────┐
│         VideoPipeline (视频流水线)           │
└──────────────┬──────────────────────────────┘
               │ cv::Mat 帧数据
               ▼
┌─────────────────────────────────────────────┐
│      IAlgorithmProcessor (算法处理器接口)     │
│  ┌──────────────┐  ┌──────────────────────┐ │
│  │ GrpcProcessor│  │ NativeCppProcessor   │ │
│  │ (Python 后端)│  │ (C++ 原生实现)       │ │
│  └──────────────┘  └──────────────────────┘ │
└──────────────┬──────────────────────────────┘
               │
        ┌──────┴──────┐
        ▼             ▼
┌──────────────┐ ┌────────────────┐
│ Preprocessor │ │ Model Inference│
│ (预处理)     │ │ (模型推理)      │
└──────────────┘ └────────────────┘
                          │
                          ▼
                  ┌──────────────┐
                  │ Postprocessor│
                  │ (后处理)      │
                  └──────────────┘
```

### 核心设计原则

1. **接口抽象**：通过 `IAlgorithmProcessor` 解耦具体实现
2. **策略模式**：运行时切换 Python/C++ 后端
3. **工厂模式**：根据配置创建对应的处理器
4. **观察者模式**：检测结果通过回调通知

---

## 模块划分

### 1. 核心接口层 (`modules/alg/include/alg/`)

```
include/alg/
├── i_algorithm.h              # 基础算法接口（已有）
├── i_algorithm_processor.h    # 处理器接口（已有）
├── algorithm_result.h         # 算法结果结构
├── bounding_box.h             # 检测框结构
└── processor_factory.h        # 处理器工厂（新增）
```

### 2. 预处理模块 (`modules/alg/preprocess/`)

```
preprocess/
├── include/
│   └── alg/preprocess/
│       ├── i_preprocessor.h       # 预处理接口
│       ├── letterbox.h            # Letterbox 缩放
│       ├── normalize.h            # 归一化
│       └── color_convert.h        # 颜色空间转换
├── src/
│   ├── letterbox.cpp
│   ├── normalize.cpp
│   └── color_convert.cpp
└── CMakeLists.txt
```

**功能**：
- Letterbox 缩放（保持宽高比）
- 像素值归一化（0-255 → 0-1）
- BGR → RGB 转换
- HWC → CHW 布局转换

### 3. 推理引擎模块 (`modules/alg/inference/`)

```
inference/
├── include/
│   └── alg/inference/
│       ├── i_inference_engine.h   # 推理引擎接口
│       ├── openvino_engine.h      # OpenVINO 引擎
│       ├── onnxruntime_engine.h   # ONNX Runtime 引擎
│       └── tensorrt_engine.h      # TensorRT 引擎（可选）
├── src/
│   ├── openvino_engine.cpp
│   ├── onnxruntime_engine.cpp
│   └── tensorrt_engine.cpp
└── CMakeLists.txt
```

**功能**：
- 模型加载（IR/ONNX/Engine）
- 输入/输出张量管理
- 同步/异步推理
- 设备选择（CPU/GPU）

### 4. 后处理模块 (`modules/alg/postprocess/`)

```
postprocess/
├── include/
│   └── alg/postprocess/
│       ├── i_postprocessor.h      # 后处理接口
│       ├── nms.h                  # 非极大值抑制
│       ├── confidence_filter.h    # 置信度过滤
│       └── box_decoder.h          # 边界框解码
├── src/
│   ├── nms.cpp
│   ├── confidence_filter.cpp
│   └── box_decoder.cpp
└── CMakeLists.txt
```

**功能**：
- 置信度阈值过滤
- NMS（非极大值抑制）
- 边界框坐标解码
- 类别映射

### 5. 具体算法实现 (`modules/alg/algorithms/`)

```
algorithms/
├── include/
│   └── alg/algorithms/
│       ├── yolov5_algorithm.h     # YOLOv5 算法
│       ├── yolov8_algorithm.h     # YOLOv8 算法（预留）
│       └── motion_detection.h     # 运动检测（已有）
├── src/
│   ├── yolov5_algorithm.cpp
│   ├── yolov8_algorithm.cpp
│   └── motion_detection.cpp
└── CMakeLists.txt
```

**功能**：
- 整合预处理、推理、后处理
- 实现 `IAlgorithm` 接口
- 提供算法特定配置

### 6. 处理器实现 (`modules/alg/processors/`)

```
processors/
├── include/
│   └── alg/processors/
│       ├── grpc_processor.h       # gRPC 处理器（已有）
│       ├── native_cpp_processor.h # C++ 原生处理器（新增）
│       └── hybrid_processor.h     # 混合处理器（可选）
├── src/
│   ├── grpc_processor.cpp
│   ├── native_cpp_processor.cpp
│   └── hybrid_processor.cpp
└── CMakeLists.txt
```

**功能**：
- `GrpcProcessor`：调用 Python 后端（保留兼容）
- `NativeCppProcessor`：直接调用 C++ 算法
- `HybridProcessor`：根据负载动态切换

### 7. 工具类模块 (`modules/alg/utils/`)

```
utils/
├── include/
│   └── alg/utils/
│       ├── timer.h                # 性能计时器
│       ├── logger.h               # 日志工具
│       ├── config_parser.h        # 配置解析
│       └── image_utils.h          # 图像工具
├── src/
│   ├── timer.cpp
│   ├── logger.cpp
│   ├── config_parser.cpp
│   └── image_utils.cpp
└── CMakeLists.txt
```

---

## TODO 列表

### Phase 1: 基础设施搭建（1-2 周）

#### 1.1 项目结构调整
- [ ] 创建新的目录结构（按上述模块划分）
- [ ] 更新顶层 `CMakeLists.txt`
- [ ] 创建各子模块的 `CMakeLists.txt`
- [ ] 配置 vcpkg 依赖（OpenVINO、ONNX Runtime）

#### 1.2 核心接口完善
- [ ] 完善 `IAlgorithmProcessor` 接口
- [ ] 定义 `AlgorithmConfig` 配置结构
- [ ] 实现 `ProcessorFactory` 工厂类
- [ ] 编写接口单元测试

#### 1.3 工具类实现
- [ ] 实现 `Timer` 性能计时器
- [ ] 实现 `Logger` 日志封装
- [ ] 实现 `ConfigParser` YAML 配置解析
- [ ] 实现 `ImageUtils` 图像工具函数

**交付物**：
- ✅ 完整的项目骨架
- ✅ 可编译的基础框架
- ✅ 单元测试覆盖核心接口

---

### Phase 2: 预处理模块（1 周）

#### 2.1 预处理接口
- [ ] 定义 `IPreprocessor` 接口
- [ ] 设计预处理流水线（Pipeline）
- [ ] 实现预处理配置结构

#### 2.2 Letterbox 缩放
- [ ] 实现 Letterbox 算法
- [ ] 支持多种插值方法
- [ ] 记录缩放比例和偏移量

#### 2.3 归一化和格式转换
- [ ] 实现像素归一化（0-1）
- [ ] 实现 BGR → RGB 转换
- [ ] 实现 HWC → CHW 布局转换
- [ ] 支持批量预处理

#### 2.4 测试和验证
- [ ] 编写单元测试
- [ ] 与 Python 版本对比验证
- [ ] 性能基准测试

**交付物**：
- ✅ 完整的预处理模块
- ✅ 与 Python 版本输出一致
- ✅ 性能优于 Python（SIMD 优化）

---

### Phase 3: 推理引擎模块（2-3 周）

#### 3.1 推理引擎接口
- [ ] 定义 `IInferenceEngine` 接口
- [ ] 设计张量数据结构
- [ ] 实现引擎配置结构

#### 3.2 OpenVINO 引擎（优先）
- [ ] 集成 OpenVINO C++ API
- [ ] 实现模型加载（IR 格式）
- [ ] 实现输入/输出张量绑定
- [ ] 实现同步推理
- [ ] 实现异步推理（多请求）
- [ ] 支持 CPU/GPU 设备切换

#### 3.3 ONNX Runtime 引擎（备选）
- [ ] 集成 ONNX Runtime C++ API
- [ ] 实现模型加载（ONNX 格式）
- [ ] 实现推理流程
- [ ] 支持 CUDA/TensorRT（可选）

#### 3.4 性能优化
- [ ] 实现推理请求池
- [ ] 内存预分配（避免频繁分配）
- [ ] 批处理支持
- [ ] 多线程推理

#### 3.5 测试和验证
- [ ] 编写单元测试
- [ ] 与 Python OpenVINO 对比
- [ ] 性能基准测试（吞吐量、延迟）

**交付物**：
- ✅ OpenVINO 推理引擎
- ✅ 支持同步/异步模式
- ✅ 性能达到或超过 Python 版本

---

### Phase 4: 后处理模块（1 周）

#### 4.1 后处理接口
- [ ] 定义 `IPostprocessor` 接口
- [ ] 设计后处理流水线

#### 4.2 NMS 实现
- [ ] 实现标准 NMS 算法
- [ ] 实现 Soft-NMS（可选）
- [ ] SIMD 优化（AVX2/AVX-512）

#### 4.3 置信度过滤
- [ ] 实现阈值过滤
- [ ] 支持动态阈值调整

#### 4.4 边界框解码
- [ ] 实现 YOLOv5 解码逻辑
- [ ] 坐标还原（相对 → 绝对）
- [ ] 类别映射

#### 4.5 测试和验证
- [ ] 编写单元测试
- [ ] 与 Python 版本对比
- [ ] 精度验证（mAP）

**交付物**：
- ✅ 完整的后处理模块
- ✅ NMS 性能优于 Python
- ✅ 检测结果与 Python 一致

---

### Phase 5: YOLOv5 算法实现（1-2 周）

#### 5.1 算法封装
- [ ] 实现 `YOLOv5Algorithm` 类
- [ ] 整合预处理、推理、后处理
- [ ] 实现 `IAlgorithm` 接口

#### 5.2 配置管理
- [ ] 实现 YOLOv5 配置结构
- [ ] 支持 YAML 配置文件
- [ ] 支持运行时参数调整

#### 5.3 异步推理
- [ ] 实现异步推理队列
- [ ] 支持多帧并发处理
- [ ] 实现结果排序

#### 5.4 性能优化
- [ ] 零拷贝优化（减少内存复制）
- [ ] 流水线并行（预处理、推理、后处理并行）
- [ ] 内存池管理

#### 5.5 测试和验证
- [ ] 端到端测试
- [ ] 精度验证（与 Python 对比）
- [ ] 性能基准测试

**交付物**：
- ✅ 完整的 YOLOv5 C++ 实现
- ✅ 精度与 Python 版本一致
- ✅ 性能提升 30%+

---

### Phase 6: 处理器实现（1 周）

#### 6.1 NativeCppProcessor
- [ ] 实现 `NativeCppProcessor` 类
- [ ] 整合 YOLOv5 算法
- [ ] 实现帧处理流水线
- [ ] 实现检测结果回调

#### 6.2 统计和监控
- [ ] 实现性能统计（FPS、延迟）
- [ ] 实现资源监控（CPU、内存）
- [ ] 实现健康检查

#### 6.3 错误处理
- [ ] 实现异常安全
- [ ] 实现自动恢复
- [ ] 实现优雅降级

#### 6.4 测试和验证
- [ ] 集成测试
- [ ] 压力测试
- [ ] 长时间运行测试

**交付物**：
- ✅ 可用的 C++ 原生处理器
- ✅ 完整的统计和监控
- ✅ 健壮的错误处理

---

### Phase 7: 集成和迁移（1-2 周）

#### 7.1 ProcessorFactory
- [ ] 实现工厂类
- [ ] 支持配置驱动创建
- [ ] 支持运行时切换

#### 7.2 VideoPipeline 集成
- [ ] 修改 `VideoPipeline` 使用新接口
- [ ] 实现平滑切换（Python → C++）
- [ ] 实现回退机制

#### 7.3 配置系统
- [ ] 扩展 `AppConfig` 支持算法配置
- [ ] 实现配置热重载
- [ ] 实现配置验证

#### 7.4 文档和示例
- [ ] 编写用户文档
- [ ] 编写开发者文档
- [ ] 提供示例代码

#### 7.5 清理和重构
- [ ] 移除废弃代码
- [ ] 优化代码结构
- [ ] 代码审查

**交付物**：
- ✅ 完整的集成方案
- ✅ 平滑迁移路径
- ✅ 完整的文档

---

### Phase 8: 测试和优化（1 周）

#### 8.1 单元测试
- [ ] 补充缺失的单元测试
- [ ] 提高代码覆盖率（>80%）
- [ ] 自动化测试脚本

#### 8.2 集成测试
- [ ] 端到端测试
- [ ] 多场景测试
- [ ] 边界条件测试

#### 8.3 性能测试
- [ ] 基准测试套件
- [ ] 性能回归测试
- [ ] profiling 和优化

#### 8.4 稳定性测试
- [ ] 7x24 小时运行测试
- [ ] 内存泄漏检测
- [ ] 压力测试

**交付物**：
- ✅ 完整的测试套件
- ✅ 性能报告
- ✅ 稳定性保证

---

## 技术选型

### 推理引擎

| 引擎 | 优势 | 劣势 | 适用场景 |
|------|------|------|----------|
| **OpenVINO** | Intel CPU/GPU 优化好、成熟稳定 | 仅支持 Intel 硬件 | ✅ **首选**（Intel 平台） |
| **ONNX Runtime** | 跨平台、支持多后端 | 性能略低于专用引擎 | 备选（AMD/NVIDIA） |
| **TensorRT** | NVIDIA GPU 极致性能 | 仅支持 NVIDIA、部署复杂 | 高性能需求 |

**推荐**：OpenVINO（与 Python 版本一致，便于对比验证）

### 构建系统

- **CMake 3.18+**：已有基础设施
- **vcpkg**：依赖管理（已有）

### 依赖库

| 库 | 用途 | vcpkg port |
|----|------|------------|
| OpenCV 4.x | 图像处理 | `opencv4` |
| OpenVINO | 推理引擎 | `openvino` |
| Boost | 工具库 | `boost` |
| spdlog | 日志 | `spdlog` |
| yaml-cpp | 配置解析 | `yaml-cpp` |
| Google Test | 单元测试 | `gtest` |

---

## 实施步骤

### Week 1-2: Phase 1 - 基础设施

**目标**：搭建项目骨架，完成核心接口

**关键任务**：
1. 创建目录结构
2. 编写 CMakeLists.txt
3. 实现核心接口
4. 配置依赖

**验收标准**：
- ✅ 项目可编译
- ✅ 核心接口有单元测试
- ✅ 依赖正确配置

---

### Week 3: Phase 2 - 预处理

**目标**：实现预处理模块，与 Python 版本对齐

**关键任务**：
1. 实现 Letterbox
2. 实现归一化
3. 实现格式转换
4. 对比验证

**验收标准**：
- ✅ 输出与 Python 一致
- ✅ 性能优于 Python
- ✅ 单元测试覆盖

---

### Week 4-6: Phase 3 - 推理引擎

**目标**：集成 OpenVINO，实现推理引擎

**关键任务**：
1. 集成 OpenVINO
2. 实现同步/异步推理
3. 性能优化
4. 对比验证

**验收标准**：
- ✅ 推理结果正确
- ✅ 性能达到预期
- ✅ 支持异步模式

---

### Week 7: Phase 4 - 后处理

**目标**：实现后处理模块

**关键任务**：
1. 实现 NMS
2. 实现置信度过滤
3. 实现边界框解码
4. 对比验证

**验收标准**：
- ✅ 检测结果与 Python 一致
- ✅ NMS 性能优秀
- ✅ 单元测试覆盖

---

### Week 8-9: Phase 5 - YOLOv5 算法

**目标**：完成 YOLOv5 C++ 实现

**关键任务**：
1. 整合三个模块
2. 实现异步推理
3. 性能优化
4. 端到端测试

**验收标准**：
- ✅ 精度与 Python 一致
- ✅ 性能提升 30%+
- ✅ 完整的测试

---

### Week 10: Phase 6 - 处理器

**目标**：实现 NativeCppProcessor

**关键任务**：
1. 实现处理器
2. 实现统计监控
3. 实现错误处理
4. 集成测试

**验收标准**：
- ✅ 处理器可用
- ✅ 统计信息准确
- ✅ 错误处理健壮

---

### Week 11-12: Phase 7 - 集成

**目标**：集成到主程序，实现平滑迁移

**关键任务**：
1. 实现 Factory
2. 修改 VideoPipeline
3. 配置系统
4. 文档编写

**验收标准**：
- ✅ 可运行时切换
- ✅ 配置灵活
- ✅ 文档完整

---

### Week 13: Phase 8 - 测试优化

**目标**：全面测试和性能优化

**关键任务**：
1. 单元测试
2. 集成测试
3. 性能测试
4. 稳定性测试

**验收标准**：
- ✅ 测试覆盖率 >80%
- ✅ 性能达标
- ✅ 7x24 稳定运行

---

## 风险评估

### 技术风险

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|----------|
| OpenVINO C++ API 复杂度高 | 中 | 中 | 充分调研文档、参考官方示例 |
| 性能不达预期 | 低 | 高 | 早期性能测试、预留优化时间 |
| 精度与 Python 不一致 | 中 | 高 | 逐步验证、数值对比测试 |
| 异步推理死锁 | 低 | 高 | 严格测试、超时机制 |

### 进度风险

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|----------|
| OpenVINO 学习曲线陡峭 | 中 | 中 | 预留学习时间、分阶段实施 |
| 调试困难 | 中 | 中 | 完善日志、单元测试 |
| 依赖冲突 | 低 | 中 | 使用 vcpkg、隔离测试 |

### 质量风险

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|----------|
| 内存泄漏 | 中 | 高 | RAII、智能指针、Valgrind 检测 |
| 线程安全问题 | 中 | 高 | 严格审查、压力测试 |
| 异常不安全 | 低 | 中 | 异常安全编码规范 |

---

## 成功指标

### 性能指标

- ✅ **吞吐量**：≥ Python 版本的 130%
- ✅ **延迟**：≤ Python 版本的 80%
- ✅ **CPU 占用**：≤ Python 版本的 90%
- ✅ **内存占用**：≤ Python 版本的 110%

### 质量指标

- ✅ **精度**：mAP 与 Python 版本差异 < 0.5%
- ✅ **稳定性**：7x24 小时无崩溃
- ✅ **测试覆盖率**：≥ 80%

### 工程指标

- ✅ **编译时间**：< 5 分钟（增量 < 30 秒）
- ✅ **二进制大小**：< 100 MB
- ✅ **部署复杂度**：单文件部署

---

## 参考资料

1. [OpenVINO C++ API 文档](https://docs.openvino.ai/latest/api_docs.html)
2. [ONNX Runtime C++ API](https://onnxruntime.ai/docs/api/c/)
3. [YOLOv5 官方实现](https://github.com/ultralytics/yolov5)
4. [C++ Best Practices](https://github.com/cpp-best-practices/cppbestpractices)

---

**文档版本**: v1.0  
**最后更新**: 2026-04-29  
**作者**: Lingma AI Assistant
