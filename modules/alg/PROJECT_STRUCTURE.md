# Inference 模块项目结构

## 📁 目录树

```
modules/alg/inference/
│
├── 📄 CMakeLists.txt                      # 构建配置
├── 📄 README.md                           # 完整文档 (284 行)
├── 📄 QUICKSTART.md                       # 快速开始指南 (282 行)
├── 📄 IMPLEMENTATION_SUMMARY.md           # 实现总结 (393 行)
├── 📄 PROJECT_STRUCTURE.md                # 项目结构（本文档）
├── 🔧 build_and_test.bat                  # Windows 编译脚本
├── 🔧 build_and_test.sh                   # Linux/Mac 编译脚本
│
├── 📂 include/alg/inference/              # 公共头文件
│   ├── 📄 i_inference_engine.h            # 核心接口定义 (96 行)
│   ├── 📄 tensor_data.h                   # 张量数据结构 (48 行)
│   ├── 📄 openvino_cpu_engine.h           # OpenVINO CPU 引擎 (102 行)
│   └── 📄 inference_engine_factory.h      # 工厂类 (26 行)
│
├── 📂 src/                                # 源代码实现
│   ├── 📄 openvino_cpu_engine.cpp         # OpenVINO 引擎实现 (326 行)
│   └── 📄 inference_engine_factory.cpp    # 工厂类实现 (72 行)
│
├── 📂 test/                               # 单元测试
│   └── 📄 test_inference.cpp              # 测试程序 (140 行)
│
└── 📂 examples/                           # 使用示例
    └── 📄 inference_example.cpp           # 完整示例 (283 行)
```

---

## 📊 文件统计

### 代码文件

| 文件 | 行数 | 类型 | 说明 |
|------|------|------|------|
| `i_inference_engine.h` | 96 | 接口 | 核心抽象接口 |
| `tensor_data.h` | 48 | 数据 | 张量数据结构 |
| `openvino_cpu_engine.h` | 102 | 头文件 | OpenVINO 引擎声明 |
| `openvino_cpu_engine.cpp` | 326 | 实现 | OpenVINO 引擎实现 |
| `inference_engine_factory.h` | 26 | 头文件 | 工厂类声明 |
| `inference_engine_factory.cpp` | 72 | 实现 | 工厂类实现 |
| `test_inference.cpp` | 140 | 测试 | 单元测试 |
| `inference_example.cpp` | 283 | 示例 | 使用示例 |
| **小计** | **1093** | - | **代码总计** |

### 文档文件

| 文件 | 行数 | 类型 | 说明 |
|------|------|------|------|
| `README.md` | 284 | 文档 | 完整使用文档 |
| `QUICKSTART.md` | 282 | 文档 | 快速开始指南 |
| `IMPLEMENTATION_SUMMARY.md` | 393 | 文档 | 实现总结 |
| `PROJECT_STRUCTURE.md` | ~200 | 文档 | 项目结构 |
| **小计** | **~1159** | - | **文档总计** |

### 构建文件

| 文件 | 行数 | 类型 | 说明 |
|------|------|------|------|
| `CMakeLists.txt` | 69 | 构建 | CMake 配置 |
| `build_and_test.bat` | 60 | 脚本 | Windows 脚本 |
| `build_and_test.sh` | 46 | 脚本 | Linux/Mac 脚本 |
| **小计** | **175** | - | **构建总计** |

---

## 🎯 核心组件关系图

```
┌─────────────────────────────────────────────┐
│          Application Code                    │
│                                              │
│  #include "alg/inference/                    │
│           inference_engine_factory.h"        │
│                                              │
│  auto engine = InferenceEngineFactory::      │
│      Create("openvino_cpu", config);         │
│  auto result = engine->Infer(input);         │
└──────────────┬──────────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────────┐
│     InferenceEngineFactory                   │
│                                              │
│  • Create(type, config)                     │
│  • RegisterCreator(name, func)              │
│                                              │
│  Registered Engines:                         │
│  ├─ "openvino_cpu" ✓                        │
│  ├─ "openvino_gpu" (TODO)                   │
│  ├─ "tensorrt" (TODO)                       │
│  └─ ...                                     │
└──────────────┬──────────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────────┐
│       IInferenceEngine (Interface)           │
│                                              │
│  • LoadModel(config)                        │
│  • Infer(input)                             │
│  • InferAsync(input, callback)              │
│  • InferBatch(inputs)                       │
│  • GetInputInfo() / GetOutputInfo()         │
│  • GetStats()                               │
└──────────────┬──────────────────────────────┘
               │ implements
               ▼
┌─────────────────────────────────────────────┐
│      OpenVinoCpuEngine                      │
│                                              │
│  Members:                                    │
│  ├─ ov::Core                                │
│  ├─ ov::CompiledModel                       │
│  ├─ std::vector<ov::InferRequest>           │
│  ├─ AsyncTask Queue                         │
│  └─ Worker Thread                           │
│                                              │
│  Features:                                   │
│  ├─ Request Pool (concurrent)               │
│  ├─ Async Mode                              │
│  ├─ Thread-safe Stats                       │
│  └─ Zero-copy Design                        │
└─────────────────────────────────────────────┘
```

---

## 🔄 数据流图

### 同步推理流程

```
User Code
    │
    ├─ 1. Create Engine
    │     InferenceEngineFactory::Create()
    │           │
    │           ▼
    │     OpenVinoCpuEngine::LoadModel()
    │           ├─ Read model (.xml + .bin)
    │           ├─ Compile for device
    │           └─ Create request pool
    │
    ├─ 2. Prepare Input
    │     TensorData::FromCpu(data, shape)
    │           │
    │           ▼
    │     [CPU Memory] input_data
    │
    ├─ 3. Infer()
    │     OpenVinoCpuEngine::Infer(input)
    │           │
    │           ▼
    │     ExecuteInference(input)
    │           ├─ Get idle request from pool
    │           ├─ Copy input to request buffer
    │           ├─ infer_request.infer()
    │           ├─ Get output tensors
    │           └─ Update statistics
    │
    └─ 4. Process Result
          InferenceOutput
                ├─ tensors (map<string, TensorData>)
                ├─ inference_time_us
                └─ success / error_message
```

### 异步推理流程

```
User Code
    │
    ├─ 1. Create Engine (async mode)
    │     config.async_mode = true
    │     config.num_requests = 4
    │           │
    │           ▼
    │     Start worker thread
    │
    ├─ 2. InferAsync(input, callback)
    │           │
    │           ▼
    │     Push task to queue
    │     Notify worker thread
    │
    │           │
    │           ▼
    │     [Worker Thread]
    │           ├─ Wait for task
    │           ├─ Pop task from queue
    │           ├─ ExecuteInference(input)
    │           └─ Call callback(result)
    │
    └─ 3. Callback invoked
          User's callback function
                └─ Process result asynchronously
```

---

## 🏗️ 类层次结构

```
IInferenceEngine (abstract interface)
    │
    ├─ LoadModel() = 0
    ├─ Infer() = 0
    ├─ InferAsync() = 0
    ├─ InferBatch() = 0
    ├─ WaitAll() = 0
    ├─ GetInputInfo() = 0
    ├─ GetOutputInfo() = 0
    ├─ GetType() = 0
    ├─ IsAvailable() = 0
    └─ GetStats() = 0
    │
    │ implements
    ▼
OpenVinoCpuEngine
    │
    ├─ ov::Core core_
    ├─ ov::CompiledModel compiled_model_
    ├─ std::vector<ov::InferRequest> infer_requests_
    ├─ std::thread worker_thread_
    ├─ std::queue<AsyncTask> task_queue_
    └─ Statistics (thread-safe)
```

---

## 📦 依赖关系

### 外部依赖

```
alg_inference
    │
    ├─ OpenVINO Runtime (required)
    │     └─ ov::Core, ov::CompiledModel, ov::InferRequest
    │
    ├─ spdlog (required)
    │     └─ SPDLOG_INFO, SPDLOG_ERROR, SPDLOG_WARN
    │
    └─ C++ Standard Library
          ├─ <thread>
          ├─ <mutex>
          ├─ <queue>
          ├─ <atomic>
          ├─ <chrono>
          └─ <functional>
```

### 内部依赖

```
alg_inference
    │
    └─ (未来) alg_preprocess
          └─ Preprocessor → TensorData
    
    └─ (未来) alg_postprocess
          └─ Postprocessor ← InferenceOutput
```

---

## 🚀 编译流程

### CMake 配置阶段

```
CMakeLists.txt
    │
    ├─ find_package(OpenVINO REQUIRED)
    │     └─ 查找 OpenVINO 安装路径
    │
    ├─ file(GLOB INFERENCE_SOURCES "src/*.cpp")
    │     └─ 收集源文件
    │
    ├─ add_library(alg_inference ...)
    │     └─ 创建静态库
    │
    ├─ target_include_directories(...)
    │     └─ 设置包含路径
    │
    └─ target_link_libraries(...)
          └─ 链接 OpenVINO、spdlog、threads
```

### 编译阶段

```
Source Files
    │
    ├─ openvino_cpu_engine.cpp
    │     └─ 编译为 .o 文件
    │
    ├─ inference_engine_factory.cpp
    │     └─ 编译为 .o 文件
    │
    └─ Linker
          └─ 链接所有 .o 文件 → libalg_inference.a/.lib
```

---

## 🧪 测试流程

### 单元测试

```
test_inference.cpp
    │
    ├─ TestOpenVinoCpuEngine()
    │     ├─ Create engine
    │     ├─ Check availability
    │     ├─ Get model info
    │     ├─ Run sync inference
    │     └─ Get statistics
    │
    └─ TestAsyncInference()
          ├─ Create async engine
          ├─ Send async requests
          └─ Wait for completion
```

### 示例程序

```
inference_example.cpp
    │
    ├─ Example_SyncInference()
    │     └─ 基本同步推理演示
    │
    ├─ Example_AsyncInference()
    │     └─ 异步并发推理演示
    │
    └─ Example_BatchInference()
          └─ 批量推理演示
```

---

## 📝 扩展指南

### 添加新引擎（以 TensorRT 为例）

#### Step 1: 创建头文件

```
include/alg/inference/tensorrt_engine.h
```

```cpp
class TensorRtEngine : public IInferenceEngine {
    // 实现所有虚函数
};
```

#### Step 2: 创建实现文件

```
src/tensorrt_engine.cpp
```

```cpp
bool TensorRtEngine::LoadModel(const InferenceConfig& config) {
    // TensorRT 模型加载逻辑
}

InferenceOutput TensorRtEngine::Infer(const TensorData& input) {
    // TensorRT 推理逻辑
}
// ... 其他方法
```

#### Step 3: 注册到工厂

```cpp
// inference_engine_factory.cpp
creators["tensorrt"] = [](const InferenceConfig& config) {
    auto engine = std::make_unique<TensorRtEngine>();
    if (!engine->LoadModel(config)) {
        return std::unique_ptr<IInferenceEngine>(nullptr);
    }
    return std::unique_ptr<IInferenceEngine>(std::move(engine));
};
```

#### Step 4: 更新 CMakeLists.txt

```cmake
# 如果需要额外的依赖
find_package(TensorRT REQUIRED)
target_link_libraries(alg_inference PRIVATE nvinfer)
```

---

## 🎓 学习路径

### 初学者

1. ✅ 阅读 `QUICKSTART.md`
2. ✅ 运行 `inference_example.cpp`
3. ✅ 查看 `test_inference.cpp` 了解测试方法

### 进阶开发者

1. ✅ 阅读 `README.md` 完整文档
2. ✅ 研究 `openvino_cpu_engine.cpp` 实现细节
3. ✅ 理解异步工作线程机制
4. ✅ 学习零拷贝设计模式

### 贡献者

1. ✅ 阅读 `IMPLEMENTATION_SUMMARY.md`
2. ✅ 理解整体架构设计
3. ✅ 参考"扩展指南"添加新功能
4. ✅ 编写单元测试和文档

---

## 🔗 相关资源

- **主项目**: `d:\file_mx\aaaaa\learncpp`
- **接口设计**: `docs/VIDEOPIPELINE_INTERFACE_DESIGN.md`
- **零拷贝架构**: `algorithm/CPP_ZERO_COPY_ARCHITECTURE.md`
- **算法迁移计划**: `docs/ALGORITHM_MIGRATION_PLAN.md`

---

**最后更新**: 2026-05-03  
**版本**: v1.0
