# VideoPipeline 重构实施路线图

## 📅 总体时间规划

**预计总工期**: 5-7 天

```
Week 1:
├── Day 1-2: Phase 1 - 基础架构重构
├── Day 3:   Phase 2 - OpenVINO 后端
├── Day 4-5: Phase 3 - OpenCV 后端
└── Day 6:   Phase 4 - gRPC 后端优化

Week 2:
└── Day 7:   Phase 5 - 集成测试与文档
```

---

## 🎯 Phase 1: 基础架构重构（2 天）

### 目标
建立策略模式基础架构，解耦算法后端

### 任务清单

#### Day 1: 接口定义与工厂模式

- [ ] **任务 1.1**: 创建 `IAlgorithmBackend` 接口
  - 文件: `include/videopipeline/i_algorithm_backend.h`
  - 内容:
    ```cpp
    class IAlgorithmBackend {
    public:
        virtual ~IAlgorithmBackend() = default;
        virtual bool initialize(const AlgorithmConfig& config) = 0;
        virtual void processFrame(const VideoFrame& frame) = 0;
        virtual void processFrame(cv::Mat&& frame, int64_t pts) = 0;
        // ... 回调设置
    };
    ```

- [ ] **任务 1.2**: 创建算法后端工厂
  - 文件: `include/videopipeline/algorithm_backend_factory.h`
  - 文件: `src/algorithm_backend_factory.cpp`
  - 功能: 根据配置创建对应的后端实例

- [ ] **任务 1.3**: 更新 `PipelineConfig` 验证逻辑
  - 文件: `include/videopipeline/pipeline_config.h`
  - 添加: `validateAlgorithmConfig()` 方法

**验收标准**:
- ✅ 接口编译通过
- ✅ 工厂可以创建空实现的后端
- ✅ 单元测试通过

---

#### Day 2: VideoPipeline 重构

- [ ] **任务 2.1**: 重构 `VideoPipeline` 头文件
  - 文件: `include/videopipeline/video_pipeline.h`
  - 修改:
    - 移除硬编码的 `GrpcVideoSender`
    - 添加 `std::unique_ptr<IAlgorithmBackend> algorithm_backend_`
    - 添加预处理组件（可选）

- [ ] **任务 2.2**: 重构初始化逻辑
  - 文件: `src/video_pipeline.cpp`
  - 修改:
    - `initialize()` 方法使用工厂创建后端
    - 根据配置选择预处理组件

- [ ] **任务 2.3**: 重构帧处理逻辑
  - 文件: `src/video_pipeline.cpp`
  - 修改:
    - `onFrameDecoded()` 调用后端接口
    - 移除硬编码的 gRPC 逻辑

- [ ] **任务 2.4**: 创建空实现后端（用于测试）
  - 文件: `include/videopipeline/backends/null_backend.h`
  - 文件: `src/backends/null_backend.cpp`
  - 功能: 不做任何处理，仅用于测试框架

**验收标准**:
- ✅ VideoPipeline 编译通过
- ✅ 可以启动流水线（无算法）
- ✅ 现有测试用例通过（或适当跳过）

---

## 🎯 Phase 2: OpenVINO 后端实现（1 天）

### 目标
实现零拷贝的 OpenVINO 推理后端

### 任务清单

#### Day 3: OpenVINO 后端开发

- [ ] **任务 3.1**: 创建 OpenVINO 后端类
  - 文件: `include/videopipeline/backends/openvino_backend.h`
  - 文件: `src/backends/openvino_backend.cpp`
  - 功能:
    - 加载 OpenVINO 模型
    - 零拷贝推理
    - 结果转换

- [ ] **任务 3.2**: 集成 TensorData 零拷贝接口
  - 依赖: `modules/alg/include/alg/inference/tensor_data.h`
  - 代码:
    ```cpp
    auto tensor = TensorData::FromRawData(
        frame.data[0], size, shape, UINT8
    );
    auto output = engine_->Infer(tensor);
    ```

- [ ] **任务 3.3**: 编写单元测试
  - 文件: `test/videopipeline/test_openvino_backend.cpp`
  - 测试用例:
    - 模型加载
    - 零拷贝推理
    - 结果正确性
    - 性能基准

- [ ] **任务 3.4**: 性能优化
  - 添加批处理支持
  - 异步推理（可选）
  - 内存池（可选）

**验收标准**:
- ✅ OpenVINO 后端编译通过
- ✅ 可以使用 YOLOv5s 模型推理
- ✅ 延迟 < 5ms (1920×1080, CPU)
- ✅ 单元测试通过率 100%

---

## 🎯 Phase 3: OpenCV 后端实现（2 天）

### 目标
实现支持预处理的 OpenCV 算法后端

### 任务清单

#### Day 4: 预处理模块

- [ ] **任务 4.1**: 设计预处理接口
  - 文件: `include/videopipeline/preprocessor.h`
  - 接口:
    ```cpp
    class IPreprocessor {
    public:
        virtual cv::Mat preprocess(const VideoFrame& frame, 
                                  const PreprocessConfig& config) = 0;
    };
    ```

- [ ] **任务 4.2**: 实现常见预处理操作
  - 文件: `src/preprocessors/resize_preprocessor.cpp`
  - 文件: `src/preprocessors/filter_preprocessor.cpp`
  - 功能:
    - Resize（缩放）
    - Gaussian Blur（高斯模糊）
    - Histogram Equalization（直方图均衡化）

- [ ] **任务 4.3**: 预处理链式调用
  - 支持多个滤镜按顺序应用
  - 配置驱动

---

#### Day 5: OpenCV 后端开发

- [ ] **任务 5.1**: 创建 OpenCV 后端类
  - 文件: `include/videopipeline/backends/opencv_backend.h`
  - 文件: `src/backends/opencv_backend.cpp`
  - 功能:
    - YUV → BGR 转换
    - 应用预处理
    - 执行 OpenCV 算法

- [ ] **任务 5.2**: 实现常见算法
  - 人脸检测（Cascade Classifier）
  - 运动检测（背景减除）
  - 目标跟踪（KCF/MOSSE）

- [ ] **任务 5.3**: 编写单元测试
  - 文件: `test/videopipeline/test_opencv_backend.cpp`
  - 测试用例:
    - 预处理正确性
    - 算法准确性
    - 性能测试

**验收标准**:
- ✅ OpenCV 后端编译通过
- ✅ 人脸检测准确率 > 90%
- ✅ 延迟 < 15ms (1920×1080)
- ✅ 单元测试通过率 100%

---

## 🎯 Phase 4: gRPC 后端优化（1 天）

### 目标
优化现有的 gRPC 后端，集成零拷贝 JPEG 编码

### 任务清单

#### Day 6: gRPC 后端优化

- [ ] **任务 6.1**: 创建 GrpcBackend 类
  - 文件: `include/videopipeline/backends/grpc_backend.h`
  - 文件: `src/backends/grpc_backend.cpp`
  - 功能:
    - 封装现有的 `GrpcVideoSender`
    - 集成 `YuvToJpegConverter`
    - 缓冲池管理

- [ ] **任务 6.2**: 集成零拷贝 JPEG 编码
  - 依赖: `modules/preprocess/include/preprocess/format_converter/yuv_to_jpeg_converter.h`
  - 代码:
    ```cpp
    size_t jpeg_size = jpeg_converter_.ConvertYuv420pZeroCopy(
        frame.data[0], frame.data[1], frame.data[2],
        frame.width, frame.height,
        buffer_.data(), buffer_.size()
    );
    ```

- [ ] **任务 6.3**: 实现缓冲池
  - 预分配 4-8 个 JPEG 缓冲区
  - 轮询复用
  - 线程安全

- [ ] **任务 6.4**: 性能测试
  - 对比旧实现和新实现
  - 测量延迟和 CPU 使用
  - 记录改进数据

**验收标准**:
- ✅ gRPC 后端编译通过
- ✅ 零拷贝编码正常工作
- ✅ 延迟降低 > 5%
- ✅ 内存分配减少 > 90%

---

## 🎯 Phase 5: 集成测试与文档（1 天）

### 目标
验证三种路径的正确性和性能，完善文档

### 任务清单

#### Day 7: 集成测试与文档

- [ ] **任务 7.1**: 端到端测试
  - 文件: `test/videopipeline/test_all_paths.cpp`
  - 测试场景:
    - 路径 A: RTSP → OpenVINO → Result
    - 路径 B: RTSP → OpenCV → Result
    - 路径 C: RTSP → gRPC → Python → Result

- [ ] **任务 7.2**: 性能对比测试
  - 测量三种路径的:
    - 端到端延迟
    - CPU 使用率
    - 内存使用
    - FPS
  - 生成对比报告

- [ ] **任务 7.3**: 编写使用文档
  - 文件: `docs/VIDEO_PIPELINE_USAGE.md`
  - 内容:
    - 快速开始
    - 配置示例
    - API 参考
    - 常见问题

- [ ] **任务 7.4**: 更新示例代码
  - 文件: `examples/openvino_example.cpp`
  - 文件: `examples/opencv_example.cpp`
  - 文件: `examples/grpc_example.cpp`

- [ ] **任务 7.5**: 代码审查与清理
  - 检查代码规范
  - 删除调试代码
  - 优化注释

**验收标准**:
- ✅ 所有测试用例通过
- ✅ 性能符合预期
- ✅ 文档完整清晰
- ✅ 示例代码可运行

---

## 📊 每日检查点

### Day 1 结束
- [ ] `IAlgorithmBackend` 接口定义完成
- [ ] 工厂模式实现完成
- [ ] 编译通过

### Day 2 结束
- [ ] `VideoPipeline` 重构完成
- [ ] 空实现后端工作正常
- [ ] 基本流水线可以启动

### Day 3 结束
- [ ] OpenVINO 后端实现完成
- [ ] 零拷贝推理工作正常
- [ ] 单元测试通过

### Day 4 结束
- [ ] 预处理模块实现完成
- [ ] 支持至少 2 种滤镜
- [ ] 预处理链式调用工作正常

### Day 5 结束
- [ ] OpenCV 后端实现完成
- [ ] 至少实现 1 种算法（人脸检测）
- [ ] 单元测试通过

### Day 6 结束
- [ ] gRPC 后端优化完成
- [ ] 零拷贝 JPEG 编码工作正常
- [ ] 性能提升验证

### Day 7 结束
- [ ] 所有集成测试通过
- [ ] 性能报告完成
- [ ] 文档和示例完成
- [ ] 代码审查通过

---

## 🔧 开发环境准备

### 依赖检查

```bash
# 确认以下库已安装
- OpenVINO (vcpkg: openvino)
- OpenCV (vcpkg: opencv)
- FFmpeg (vcpkg: ffmpeg)
- Boost (vcpkg: boost)
- gRPC (vcpkg: grpc)
```

### 构建配置

```bash
cd out/build/x64-Debug
cmake --build . --config Debug

# 确保以下模块已启用
- BUILD_VIDEOPIPELINE_TESTS=ON
- BUILD_PREPROCESS_TESTS=ON
- BUILD_ALG_TESTS=ON
```

---

## ⚠️ 注意事项

### 1. 分支管理

```bash
# 创建特性分支
git checkout -b feature/videopipeline-refactoring

# 每个 Phase 完成后提交
git commit -m "Phase 1: Basic architecture refactoring"
git commit -m "Phase 2: OpenVINO backend implementation"
# ...
```

### 2. 向后兼容

- 保留旧的 API 作为别名（标记为 deprecated）
- 提供迁移指南
- 在下一个大版本中移除旧 API

### 3. 测试优先

- 每个任务都要有对应的测试
- TDD（测试驱动开发）
- 持续集成（CI）检查

### 4. 文档同步

- 代码变更时同步更新文档
- 添加代码注释
- 记录设计决策

---

## 📈 进度跟踪

### 使用表格跟踪进度

| Phase | 任务 | 状态 | 开始日期 | 完成日期 | 备注 |
|-------|------|------|---------|---------|------|
| 1 | 1.1 接口定义 | ⏳ Pending | | | |
| 1 | 1.2 工厂模式 | ⏳ Pending | | | |
| 1 | 1.3 配置验证 | ⏳ Pending | | | |
| 1 | 2.1 头文件重构 | ⏳ Pending | | | |
| 1 | 2.2 初始化逻辑 | ⏳ Pending | | | |
| 1 | 2.3 帧处理逻辑 | ⏳ Pending | | | |
| 1 | 2.4 空实现后端 | ⏳ Pending | | | |
| 2 | 3.1 OpenVINO 后端 | ⏳ Pending | | | |
| 2 | 3.2 零拷贝集成 | ⏳ Pending | | | |
| 2 | 3.3 单元测试 | ⏳ Pending | | | |
| 2 | 3.4 性能优化 | ⏳ Pending | | | |
| 3 | 4.1 预处理接口 | ⏳ Pending | | | |
| 3 | 4.2 预处理实现 | ⏳ Pending | | | |
| 3 | 4.3 链式调用 | ⏳ Pending | | | |
| 3 | 5.1 OpenCV 后端 | ⏳ Pending | | | |
| 3 | 5.2 算法实现 | ⏳ Pending | | | |
| 3 | 5.3 单元测试 | ⏳ Pending | | | |
| 4 | 6.1 GrpcBackend | ⏳ Pending | | | |
| 4 | 6.2 零拷贝编码 | ⏳ Pending | | | |
| 4 | 6.3 缓冲池 | ⏳ Pending | | | |
| 4 | 6.4 性能测试 | ⏳ Pending | | | |
| 5 | 7.1 端到端测试 | ⏳ Pending | | | |
| 5 | 7.2 性能对比 | ⏳ Pending | | | |
| 5 | 7.3 使用文档 | ⏳ Pending | | | |
| 5 | 7.4 示例代码 | ⏳ Pending | | | |
| 5 | 7.5 代码审查 | ⏳ Pending | | | |

**状态说明**:
- ⏳ Pending: 待开始
- 🔄 In Progress: 进行中
- ✅ Done: 已完成
- ❌ Blocked: 受阻

---

## 🎉 完成标志

当以下条件全部满足时，重构完成：

1. ✅ 所有 Phase 的任务都标记为 Done
2. ✅ 所有测试用例通过（覆盖率 > 80%）
3. ✅ 性能指标达到预期
4. ✅ 文档完整且清晰
5. ✅ 代码审查通过
6. ✅ 示例代码可运行
7. ✅ 向后兼容性得到保证

---

**创建日期**: 2026-05-04  
**作者**: Lingma AI Assistant  
**版本**: v1.0
