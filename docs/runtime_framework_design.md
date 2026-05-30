# Runtime Framework 设计文档

> 本文档详细描述项目中 `common` 模块中的运行时框架（Runtime Framework）实现，
> 涵盖 Node 模型、Mailbox 通信、Executor 执行器、Scheduler 调度器、Runtime 协调器，
> 以及两套完整实现（教学版 Runtime 和生产级 AsioRuntime），
> 并扩展说明 Application 框架、IService 接口层和锁无关队列工具。

---

## 目录

1. [架构概述](#1-架构概述)
2. [核心抽象层](#2-核心抽象层)
   - 2.1 [Node（节点）](#21-node-节点)
   - 2.2 [Mailbox（信箱）](#22-mailbox-信箱)
   - 2.3 [Executor（执行器）](#23-executor-执行器)
   - 2.4 [Scheduler（调度器）](#24-scheduler-调度器)
   - 2.5 [Runtime（运行时协调器）](#25-runtime-运行时协调器)
3. [Teaching Runtime（教学版）](#3-teaching-runtime-教学版)
   - 3.1 [ThreadPoolExecutor](#31-threadpoolexecutor)
   - 3.2 [Scheduler 调度逻辑](#32-scheduler-调度逻辑)
   - 3.3 [Runtime 协调流程](#33-runtime-协调流程)
4. [Asio Runtime（生产级）](#4-asio-runtime-生产级)
   - 4.1 [AsioExecutor](#41-asioexecutor)
   - 4.2 [AsioScheduler 与 Strand](#42-asioscheduler-与-strand)
   - 4.3 [AsioRuntime](#43-asioruntime)
5. [无锁队列工具集](#5-无锁队列工具集)
   - 5.1 [BoundedSpscQueue / UnboundedSpscQueue](#51-boundedspscqueue--unboundedSpscQueue)
   - 5.2 [BoundedMpscQueue / UnboundedMpscQueue](#52-boundedmpscqueue--unboundedmpscqueue)
6. [Application 框架](#6-application-框架)
   - 6.1 [Application 单例](#61-application-单例)
   - 6.2 [ServiceContainer](#62-servicecontainer)
   - 6.3 [SignalHandler](#63-signalhandler)
7. [IService 接口层](#7-iservice-接口层)
8. [完整数据流示例](#8-完整数据流示例)
9. [依赖与构建集成](#9-依赖与构建集成)

---

## 1. 架构概述

运行时框架设计遵循 **Actor 模型** 与 **管道-过滤器** 混合模式。每一级抽象解决一个关注点：

| 层级 | 组件 | 职责 |
|------|------|------|
| 数据生产者 | `ISourceNode` / `INode` | 定义数据处理逻辑 + 向下游发射 |
| 通信 | `Mailbox` | 线程间数据传递 + 背压控制 |
| 调度 | `Executor` | 线程资源管理（真实的线程或 Asio io_context） |
| 调度策略 | `Scheduler` | 节点唤醒、批量 Drain、错误处理 |
| 协调器 | `Runtime` / `AsioRuntime` | 生命周期管理、执行图构建、外部注入 |

```
+──────────+     EmitCallback     +──────────+
| ISource  |─────────────────────>|  INode   |
| (主动)    |                      | (被动)    |
+──────────+                      +──────────+
      │                                │
      │ Emit(frame)                    │ SetEmitCallback
      ▼                                ▼
+─────────────+              +──────────────────+
|  Runtime    |              | Scheduler         |
|  - edges_   |─ frame ───>  | - Enqueue()       |
|  - nodes_   |              | - Schedule()      |
|  - sources_ |              | - Drain()         |
+─────────────+              +──────────────────+
                                    │
                                    │ executor->Post()
                                    ▼
                            +──────────────+
                            │  Executor     │
                            │  (Thread/Asio)│
                            +──────────────+
```

**两套实现对比：**

| 特性 | Teaching Runtime | Asio Runtime |
|------|------------------|--------------|
| 命名空间 | `common::thread` | `common::thread::asio` |
| Executor | `IExecutor` / `ThreadPoolExecutor` | `AsioExecutor` |
| 调度 | CAS 单标识 + 批量 Drain | CAS + Boost.Asio `strand` 串行化 |
| 线程模型 | `std::thread` + condition_variable | `boost::asio::io_context` + work_guard |
| Node 上下文 | `NodeContext` | `AsioNodeContext`（含 strand） |
| 适用场景 | 学习、原型验证 | 生产环境、多线程管线 |

---

## 2. 核心抽象层

### 2.1 Node（节点）

**文件：** `include/common/thread/node.h`

#### `INode<Frame>` — 被动处理节点

```cpp
template <typename Frame>
class INode {
public:
    virtual void Process(Frame frame) = 0;       // 处理单帧数据
    virtual void SetEmitCallback(EmitCallback<Frame> emit);  // 注册下游发射器
protected:
    void Emit(Frame frame) const;  // 向下游发送数据
};
```

- `Process()` — 纯虚接口，子类实现具体的数据处理逻辑
- `Emit()` — 回调下游节点（通过 `SetEmitCallback` 注册的闭包）
- `EmitCallback<Frame>` — 类型别名：`std::function<void(Frame)>`

#### `ISourceNode<Frame>` — 主动源节点

```cpp
template <typename Frame>
class ISourceNode {
public:
    virtual void Start() = 0;
    virtual void Stop() = 0;
    virtual void SetEmitCallback(EmitCallback<Frame> emit);
protected:
    void Emit(Frame frame) const;
};
```

- `Start()` / `Stop()` — Runtime 在生命周期中自动调用
- 典型场景：摄像头读取、文件解码、网络接收等主动产出数据的模块

**设计意图：**
将"数据处理"（`INode::Process`）与"数据传递"（`Emit`）解耦。节点本身不关心下游是谁，也不关心数据如何被调度执行。

---

### 2.2 Mailbox（信箱）

**文件：** `include/common/thread/mailbox.h`

Mailbox 是节点间异步通信的通道。框架提供两种实现：`SPSCMailBox`（默认，无锁高性能）和 `MPMCMailBox`（兼容多生产者语义），通过 `MailBoxKind` 枚举在 `NodeOptions` 中选择。

#### 枚举类型

```cpp
enum class BackpressurePolicy {
    Block,        // 阻塞生产者直到队列有空位
    DropNewest,   // 丢弃新到达的数据
    DropOldest,   // 丢弃队列中最旧的数据
    Unbounded     // 无限制增长（内存无上限）
};

enum class MailboxPushResult {
    Accepted,
    DroppedNewest,
    DroppedOldest,
    Closed
};

enum class MailBoxKind {
    SPSC,   // 单生产者-单消费者（默认，无锁路径）
    MPMC    // 多生产者-多消费者（互斥锁保护）
};
```

#### 抽象接口 `IMailBox<T>`

```cpp
template <typename T>
class IMailBox {
public:
    virtual ~IMailBox() = default;

    virtual MailboxPushResult Push(T item, BackpressurePolicy policy) = 0;
    virtual bool TryPop(T& item) = 0;          // 非阻塞弹出
    virtual bool WaitPop(T& item) = 0;         // 阻塞等待弹出
    virtual void Close() = 0;                   // 关闭（唤醒所有等待者）
    virtual void Open() = 0;                    // 重新打开
    virtual void Clear() = 0;                   // 清空
    virtual bool Empty() const = 0;
    virtual std::size_t Size() const = 0;
    virtual std::size_t Capacity() const = 0;
    virtual bool IsClosed() const = 0;
};
```

#### `SPSCMailBox<T>` — 默认实现（无锁高性能）

适用于**单生产者-单消费者**场景（节点间边的默认模式）。

| 路径 | 底层实现 | 锁 |
|------|----------|----|
| `DropNewest` / `DropOldest` | `BoundedSpscQueue<T>`（`boost::lockfree::spsc_queue` 环形缓冲区） | **无锁** |
| `Unbounded`（容量为 0） | `UnboundedSpscQueue<T>`（`std::deque` + `std::mutex`） | 有锁，永不丢帧 |
| `Block` | 自旋等待 + `std::condition_variable` | 条件变量等待时加锁 |

```cpp
MailboxPushResult Push(T item, BackpressurePolicy policy) {
    if (closed_.load()) return MailboxPushResult::Closed;

    if (unbounded_) {
        unbounded_->push(std::move(item));
        data_cv_.notify_one();
        return MailboxPushResult::Accepted;
    }

    if (policy == Block || policy == Unbounded) {
        while (!closed_.load()) {
            if (!bounded_->full() && bounded_->push(std::move(item))) {
                data_cv_.notify_one();
                return MailboxPushResult::Accepted;
            }
            std::unique_lock<std::mutex> lock(wait_mutex_);
            space_cv_.wait(lock, [this]() { return closed_ || !bounded_->full(); });
        }
        return MailboxPushResult::Closed;
    }

    if (!bounded_->full() && bounded_->push(std::move(item))) {
        data_cv_.notify_one();
        return MailboxPushResult::Accepted;
    }
    if (policy == DropOldest) {
        T dropped{};
        if (bounded_->pop(dropped) && bounded_->push(std::move(item))) {
            data_cv_.notify_one();
            return MailboxPushResult::DroppedOldest;
        }
    }
    return MailboxPushResult::DroppedNewest;
}

bool TryPop(T& item) {
    bool popped = unbounded_ ? unbounded_->try_pop(item) : bounded_->pop(item);
    if (popped) space_cv_.notify_one();
    return popped;
}
```

#### `MPMCMailBox<T>` — 多生产者兼容实现

适用于多生产者-多消费者场景（如 Fan-in 汇聚节点）。

```cpp
template <typename T>
class MPMCMailBox : public IMailBox<T> {
    // 内部数据结构：std::deque<T> + std::mutex + std::condition_variable
    // 行为语义完全等同于之前版本的 Mailbox<T>
    // 所有操作均有锁保护
};
```

#### 工厂函数

```cpp
template <typename T>
std::unique_ptr<IMailBox<T>> CreateMailBox(MailBoxKind kind, std::size_t capacity);
```

`NodeContext` / `AsioNodeContext` 的构造函数根据 `NodeOptions::mailbox_kind` 自动调用此工厂创建对应实现。

#### 设计要点

| 策略 | SPSCMailBox 行为 | MPMCMailBox 行为 |
|------|------------------|------------------|
| `Block` | 条件变量等待, **Push 有锁**, TryPop 无锁 | 条件变量等待, **全部有锁** |
| `DropNewest` | **完全无锁** | 有锁 |
| `DropOldest` | **完全无锁**（仅满时 pop + push） | 有锁 |
| `Unbounded` | UnboundedSpscQueue 有锁 | 有锁 |

---

### 2.3 Executor（执行器）

**文件：** `include/common/thread/executor.h`

#### `IExecutor` 抽象接口

```cpp
class IExecutor {
public:
    using Task = std::function<void()>;
    virtual void Start() = 0;      // 启动线程池
    virtual void Stop() = 0;       // 停止所有线程
    virtual bool Post(Task task) = 0;  // 提交任务
    virtual const std::string& Name() const = 0;
    virtual std::size_t Pending() const = 0;  // 待处理任务数
};
```

#### `ThreadPoolExecutor` — 完整实现

```cpp
class ThreadPoolExecutor : public IExecutor {
    // Start(): 创建 thread_count_ 个线程，每个执行 WorkerLoop()
    // Stop():   设置 stopping_=true，notify_all，join 所有线程
    // Post():   tasks_.push_back(task) + cv_.notify_one()
    // Pending(): tasks_.size() + active_tasks_

    void WorkerLoop() {
        while (true) {
            Task task;
            {
                unique_lock lock(mutex_);
                cv_.wait(lock, [this]() { return stopping_ || !tasks_.empty(); });
                if (stopping_ && tasks_.empty()) return;
                task = std::move(tasks_.front());
                tasks_.pop_front();
                active_tasks_.fetch_add(1);
            }
            try { task(); } catch (...) { /* 吞掉异常 */ }
            active_tasks_.fetch_sub(1);
        }
    }
};
```

**派生类：**

| 类名 | 线程数 | 默认名称 | 用途 |
|------|--------|----------|------|
| `SingleThreadExecutor` | 1 | `"single"` | 默认执行器 |
| `ThreadPoolExecutor` | N | 构造指定 | CPU 密集型计算 |
| `InferenceExecutor` | 1 | `"inference"` | AI 推理任务 |
| `IOExecutor` | 1 | `"io"` | 文件/网络 IO |

**WatchDog 特性：** 异常被 `catch(...)` 吞掉，确保工作线程永不因未捕获异常而退出。

---

### 2.4 Scheduler（调度器）

**文件：** `include/common/thread/scheduler.h`

Scheduler 是连接 Mailbox 与 Executor 的桥梁，负责"何时执行哪个节点的 Process"。

#### 节点上下文

```cpp
struct NodeContext<Frame> {
    std::string id;                           // 节点唯一标识
    std::shared_ptr<INode<Frame>> node;       // 节点实例
    IExecutor* executor{nullptr};             // 绑定的执行器
    NodeOptions options;                      // 节点选项
    std::unique_ptr<IMailBox<Frame>> mailbox; // 输入队列（由 CreateMailBox 工厂创建）
    NodeMetrics metrics;                      // 运行时指标
    std::atomic_bool scheduled{false};        // 是否已调度（防重复调度）
};
```

构造函数自动调用 `CreateMailBox<Frame>(options.mailbox_kind, ...)` 创建对应类型的 Mailbox 实例。

#### 节点选项

```cpp
struct NodeOptions {
    std::string executor_name{"single"};          // 执行器名称
    std::size_t mailbox_capacity{64};             // Mailbox 容量
    std::size_t max_batch_size{64};               // 每批最大处理数
    MailBoxKind mailbox_kind{MailBoxKind::SPSC};  // Mailbox 类型（SPSC/MPMC）
    BackpressurePolicy backpressure{BackpressurePolicy::DropOldest};
};
```

#### 指标系统

```cpp
struct NodeMetrics {
    atomic<uint64_t> enqueued;    // 入队总数
    atomic<uint64_t> processed;   // 处理成功总数
    atomic<uint64_t> dropped;     // 丢弃总数
    atomic<uint64_t> rejected;    // 拒绝总数（Mailbox 已关闭）
    atomic<uint64_t> errors;      // 处理异常总数

    NodeMetricsSnapshot Snapshot() const;  // 原子快照
};
```

#### Scheduler 核心调度流程

```cpp
class Scheduler<Frame> {
    // Step 1: Enqueue — 推入 Mailbox
    bool Enqueue(Context& ctx, Frame frame) {
        auto result = ctx.mailbox->Push(std::move(frame), ctx.options.backpressure);
        // 更新 metrics
        switch (result) {
            case Accepted:     ctx.metrics.enqueued++; break;
            case DroppedOldest: ctx.metrics.enqueued++; ctx.metrics.dropped++; break;
            case DroppedNewest: ctx.metrics.dropped++; return false;
            case Closed:        ctx.metrics.rejected++; return false;
        }
        Schedule(&ctx);
        return true;
    }

    // Step 2: Schedule — CAS 避免重复调度
    void Schedule(Context* ctx) {
        bool expected = false;
        if (!ctx->scheduled.compare_exchange_strong(expected, true)) return;
        if (!ctx->executor->Post([this, ctx]() { Drain(ctx); })) {
            ctx->scheduled.store(false);
            ctx->metrics.rejected++;
        }
    }

    // Step 3: Drain — 批量处理
    void Drain(Context* ctx) {
        for (size_t i = 0; i < ctx->options.max_batch_size; ++i) {
            Frame frame;
            if (!ctx->mailbox->TryPop(frame)) break;
            try {
                ctx->node->Process(std::move(frame));
                ctx->metrics.processed++;
            } catch (...) {
                ctx->metrics.errors++;
                if (error_handler_) error_handler_(ctx->id, std::current_exception());
            }
        }
        ctx->scheduled.store(false);
        if (!ctx->mailbox->Empty()) Schedule(ctx);  // 还有数据，继续调度
    }
};
```

**关键设计点：**

1. **CAS 防重入：** `scheduled` 标志位防止同一个节点的多个 Drain 任务被重复提交到 Executor，确保同一时刻最多有一个 Drain 在运行
2. **批量 Drain：** `max_batch_size` 限制单次处理量，防止一个节点独占线程池太久
3. **自愈调度：** Drain 结束后若 Mailbox 中还有数据，自动触发下一轮调度
4. **异常隔离：** 节点的异常被捕获并转交给 `error_handler_`，不影响其他节点的正常执行

---

### 2.5 Runtime（运行时协调器）

**文件：** `include/common/thread/runtime.h`

Runtime 是整个框架的顶层入口，管理节点的注册、连接、生命周期和外部数据注入。

#### 核心成员

```cpp
class Runtime<Frame> {
    unordered_map<string, unique_ptr<IExecutor>> executors_;  // 执行器池
    unordered_map<NodeId, unique_ptr<Context>> nodes_;        // 节点池
    unordered_map<NodeId, SourcePtr> sources_;                // 源节点池
    unordered_map<NodeId, vector<NodeId>> edges_;             // 有向边图
    Scheduler<Frame> scheduler_;                              // 调度器
    bool running_{false};
    mutable mutex mutex_;
};
```

#### 节点生命周期

```
┌─────────┐    ┌──────────┐    ┌───────────┐    ┌──────────┐
│ Register │───>│  Start   │───>│  Running  │───>│  Stop    │
│ AddNode/ │    │ 打开Mail │    │ Push/Emit │    │ 关闭Mail │
│ AddSource│    │ 启动Exec │    │ Drain处理 │    │ 停Exec   │
│ Connect  │    │ 启动Src  │    │           │    │ 停Src    │
└─────────┘    └──────────┘    └───────────┘    └──────────┘
```

#### 关键接口实现

```cpp
// 添加节点，自动注册 EmitCallback 闭包
bool AddNode(NodeId id, NodePtr node, NodeOptions options = {}) {
    auto executor = FindExecutorLocked(options.executor_name);
    if (!executor) return false;

    node->SetEmitCallback([this, id](Frame frame) { Emit(id, std::move(frame)); });

    auto context = make_unique<Context>(id, node, executor, options);
    nodes_.emplace(context->id, move(context));
    return true;
}

// 添加源节点，同样注册 EmitCallback 闭包
bool AddSource(NodeId id, SourcePtr source) { ... }

// 连接两个节点（有向边）
bool Connect(const NodeId& from, const NodeId& to) {
    edges_[from].push_back(to);
}

// 外部数据注入
bool Push(const NodeId& to, Frame frame) {
    return scheduler_.Enqueue(*nodes_[to], move(frame));
}

// 内部发射（节点运行时调用 Emit，触发此函数）
bool Emit(const NodeId& from, Frame frame) {
    auto& downstream = edges_[from];
    for (auto& to : downstream) {
        scheduler_.Enqueue(*nodes_[to], frame);
    }
}

// 生命周期控制
bool Start() {
    for (auto& [_, ctx] : nodes_) ctx->mailbox->Open();
    for (auto& [_, exec] : executors_) exec->Start();
    running_ = true;
    for (auto& [_, src] : sources_) src->Start();
}

void Stop() {
    running_ = false;
    for (auto& [_, src] : sources_) src->Stop();
    for (auto& [_, ctx] : nodes_) ctx->mailbox->Close();
    for (auto* exec : executors) exec->Stop();
}
```

**设计要点：**

- `AddNode()` 自动注册 `EmitCallback`，将节点的输出连接到 Runtime 的 `Emit()` 方法
- `Emit()` 查边图找到下游节点列表，调用 `Scheduler::Enqueue` 将数据路由过去
- 启动顺序：Mailbox Open → Executor Start → Source Start
- 停止顺序：Source Stop → Mailbox Close → Executor Stop

---

## 3. Teaching Runtime（教学版）

**聚合头文件：** `include/common/thread/runtime_framework.h`

### 3.1 ThreadPoolExecutor

详见 [2.3 Executor](#23-executor-执行器)。实现特点：

- **任务队列：** `std::deque<Task>` + `std::mutex` + `std::condition_variable`
- **线程管理：** 析构时自动调用 `Stop()` 确保线程被 join
- **异常安全：** WorkerLoop 中的 `try { task(); } catch (...) { }` 防止异常逃逸

### 3.2 Scheduler 调度逻辑

详见 [2.4 Scheduler](#24-scheduler-调度器)。与 Asio 版的关键区别：

- 没有 strand 机制，`Drain` 直接在 Executor 的线程上执行
- 任务提交路径：`Scheduler::Schedule` → `executor->Post(DrainTask)` → 线程池分配线程 → `Drain()`

### 3.3 Runtime 协调流程

详见 [2.5 Runtime](#25-runtime-运行时协调器)。

**典型使用方式：**

```cpp
#include "common/thread/runtime_framework.h"

struct Frame { int data; };

class MultiplyNode : public common::thread::INode<Frame> {
    void Process(Frame frame) override {
        frame.data *= 2;
        Emit(std::move(frame));
    }
};

class CollectNode : public common::thread::INode<Frame> {
    void Process(Frame frame) override {
        results_.push_back(frame.data);
    }
    std::vector<int> results_;
};

class RangeSource : public common::thread::ISourceNode<Frame> {
    void Start() override {
        for (int i = 0; i < 10; ++i)
            Emit(Frame{i});
    }
    void Stop() override {}
};

// 构建管线
common::thread::Runtime<Frame> rt;
rt.AddDefaultExecutors();                              // cpu + inference + io
rt.AddNode("multiply", std::make_shared<MultiplyNode>(),
           {.executor_name = "cpu"});
rt.AddNode("collect", std::make_shared<CollectNode>());
rt.AddSource("source", std::make_shared<RangeSource>());
rt.Connect("source", "multiply");
rt.Connect("multiply", "collect");
rt.Start();   // 自动开始处理
rt.Stop();    // 停止
```

---

## 4. Asio Runtime（生产级）

**聚合头文件：** `include/common/thread/asio_runtime_framework.h`

### 4.1 AsioExecutor

**文件：** `include/common/thread/asio_executor.h`

#### 核心实现

```cpp
class AsioExecutor {
public:
    using IOContext = boost::asio::io_context;
    using WorkGuard = boost::asio::executor_work_guard<IOContext::executor_type>;

    void Start() {
        io_context_.restart();
        work_guard_.emplace(make_work_guard(io_context_));
        accepting_.store(true);
        for (size_t i = 0; i < thread_count_; ++i)
            workers_.emplace_back([this]() { io_context_.run(); });
    }

    void Stop() {
        accepting_.store(false);
        work_guard_.reset();       // 允许 io_context::run() 退出
        boost::asio::post(io_context_, [](){});  // 唤醒所有线程
        for (auto& w : workers_) if (w.joinable()) w.join();
    }

    bool Post(Task task) {
        pending_.fetch_add(1);
        boost::asio::post(io_context_, [this, task = std::move(task)]() mutable {
            try { task(); } catch (...) { }
            pending_.fetch_sub(1);
        });
    }
};
```

**与 ThreadPoolExecutor 的关键区别：**

| 特性 | ThreadPoolExecutor | AsioExecutor |
|------|-------------------|--------------|
| 事件驱动 | ❌ 手动 wait/notify | ✅ io_context 事件循环 |
| Work Guard | ❌ | ✅ 防止 io_context 空转退出 |
| `GetIOContext()` | ❌ | ✅ 获取底层 io_context |
| Strand 支持 | ❌ | ✅ 配合 boost::asio::strand |
| 任务计数 | `mutex` + `deque` | `atomic pending` + io_context 内部队列 |
| 线程管理 | 自行管理 | io_context.run() 统一管理 |

#### `GetIOContext()` — 暴露底层 io_context

这是 `AsioScheduler` 创建 strand 的基础，也是与其他 Asio 组件（如网络层）共享事件循环的通道。

#### 派生类

```cpp
class SingleThreadAsioExecutor;  // 1 线程
class CpuAsioExecutor;           // hardware_concurrency()
class InferenceAsioExecutor;     // 1 线程
class IOAsioExecutor;            // 1 线程
```

### 4.2 AsioScheduler 与 Strand

**文件：** `include/common/thread/asio_scheduler.h`

#### AsioNodeContext — 扩展上下文

```cpp
template <typename Frame>
struct AsioNodeContext {
    using Strand = boost::asio::strand<boost::asio::io_context::executor_type>;

    // ... 同 NodeContext 的字段 ...
    std::unique_ptr<IMailBox<Frame>> mailbox;  // 由 CreateMailBox 工厂创建
    Strand strand;  // 额外的 strand，确保节点串行执行
};
```

构造函数自动调用 `CreateMailBox<Frame>(options.mailbox_kind, ...)` 创建对应类型的 Mailbox。

`Strand` 从 Executor 的 `io_context.get_executor()` 构造，保证所有通过该 strand 投递的任务在同一个执行器上**串行**执行，无需额外加锁。

#### AsioScheduler — 调度逻辑

```cpp
template <typename Frame>
class AsioScheduler {
    bool Enqueue(Context& ctx, Frame frame) {
        auto result = ctx.mailbox->Push(std::move(frame), ctx.options.backpressure);
        // 更新 metrics
        switch (result) {
            case Accepted:     ctx.metrics.enqueued++; break;
            case DroppedOldest: ctx.metrics.enqueued++; ctx.metrics.dropped++; break;
            case DroppedNewest: ctx.metrics.dropped++; return false;
            case Closed:        ctx.metrics.rejected++; return false;
        }
        Schedule(&ctx);
        return true;
    }

    void Schedule(Context* ctx) {
        bool expected = false;
        if (!ctx->scheduled.compare_exchange_strong(expected, true)) return;

        // 注意：两层 Post
        ctx->executor->Post([this, ctx]() {
            // 第一层：投递到 io_context 的任意线程
            boost::asio::post(ctx->strand, [this, ctx]() {
                // 第二层：通过 strand 串行化 Drain
                Drain(ctx);
            });
        });
    }

    void Drain(Context* ctx) {
        std::size_t processed_in_batch = 0;
        Frame frame{};
        while (ctx->mailbox->TryPop(frame)) {
            try {
                ctx->node->Process(std::move(frame));
                ctx->metrics.processed++;
            } catch (...) {
                ctx->metrics.errors++;
                if (error_handler_) error_handler_(ctx->id, std::current_exception());
            }
            if (++processed_in_batch >= ctx->options.max_batch_size) break;
        }
        ctx->scheduled.store(false);
        if (!ctx->mailbox->Empty()) Schedule(ctx);
    }
};
```

**为什么需要两层 Post？**

1. 外层 `executor->Post()` — 将任务提交到 Executor 的 io_context，更新 pending 计数器
2. 内层 `boost::asio::post(ctx->strand, ...)` — 通过 strand 对 Drain 进行串行化，确保同一节点永远不会并发执行

### 4.3 AsioRuntime

**文件：** `include/common/thread/asio_runtime.h`

接口与 `Runtime` **完全相同**，只是内部使用 `AsioExecutor`、`AsioNodeContext`、`AsioScheduler`。

```cpp
template <typename Frame>
class AsioRuntime {
    // AddExecutor, AddNode, AddSource, Connect, Start, Stop, Push, Emit, GetMetrics
    // 签名完全同 Runtime
};
```

**使用方式参考：**

```cpp
#include "common/thread/asio_runtime_framework.h"

struct Frame { cv::Mat image; };

class DecodeNode : public common::thread::INode<Frame> {
    void Process(Frame frame) override {
        frame.image = decode(frame.image);
        Emit(std::move(frame));
    }
};

class InferNode : public common::thread::INode<Frame> {
    void Process(Frame frame) override {
        auto result = model_->infer(frame.image);
        Emit(Frame{result});
    }
};

// AsioRuntime 生产级管线
common::thread::asio::AsioRuntime<Frame> rt;
rt.AddDefaultExecutors();                         // cpu / inference / io
rt.AddNode("decode",  std::make_shared<DecodeNode>(),
           {.executor_name = "cpu", .backpressure = BackpressurePolicy::DropOldest});
rt.AddNode("infer",   std::make_shared<InferNode>(),
           {.executor_name = "inference"});
rt.AddNode("collect", std::make_shared<CollectNode>());
rt.Connect("decode", "infer");
rt.Connect("infer", "collect");
rt.Start();

// 外部注入
rt.Push("decode", Frame{read_frame()});
```

---

## 5. 无锁队列工具集

### 5.1 BoundedSpscQueue / UnboundedSpscQueue

**文件：** `include/common/thread/spsc_queue.h` | 命名空间：`common`

基于 `boost::lockfree::spsc_queue` 和 `std::deque`，适用于**单生产者-单消费者**场景。

#### BoundedSpscQueue<T>

```cpp
template<typename T>
class BoundedSpscQueue {
    // 构造：固定容量，使用 boost::lockfree::spsc_queue（无锁 CAS 环形缓冲区）
    explicit BoundedSpscQueue(size_t capacity);

    bool push(const T&);     // 非阻塞，满时返回 false
    bool push(T&&);          // 移动语义
    bool pop(T&);            // 非阻塞，空时返回 false
    bool empty() const;
    bool full() const;
    size_t size() const;     // read_available()
    size_t available() const; // write_available()
    size_t capacity() const;
    void clear();            // 循环 pop 直到空
};
```

**内部实现：** `boost::lockfree::spsc_queue<T>` 是单生产者-单消费者的无锁环形缓冲区，底层使用 CAS 操作实现高性能入队出队，无阻塞。

**使用场景：** 确定只有一个生产者和一个消费者的高性能管道，如音频采样线程→处理线程。

#### UnboundedSpscQueue<T>

```cpp
template<typename T>
class UnboundedSpscQueue {
    void push(const T&);           // 永不失败
    void push(T&&);
    bool pop(T&);                  // 阻塞等待
    bool pop_for(T&, const chrono::milliseconds&);  // 超时等待
    bool try_pop(T&);               // 非阻塞
    size_t size() const;
    bool empty() const;
    void clear();
    void close();                  // 关闭（唤醒所有等待者）
    void reset();                  // 重置
};
```

**内部实现：** `std::deque<T>` + `std::mutex` + `std::condition_variable`

**使用场景：** 不允许丢包的单生产者-单消费者管道，如日志写入线程。

### 5.2 BoundedMpscQueue / UnboundedMpscQueue

**文件：** `include/common/thread/mpsc_queue.h` | 命名空间：`common`

基于 `boost::lockfree::queue`，适用于**多生产者-单消费者**场景。

#### BoundedMpscQueue<T>

```cpp
template<typename T>
class BoundedMpscQueue {
    explicit BoundedMpscQueue(size_t capacity);
    bool push(const T&);      // 多生产者安全，满时返回 false
    bool pop(T&);             // 仅消费者线程调用
    bool empty() const;
    size_t size() const;      // 近似值，通过 atomic 计数
    void clear();
};
```

**内部实现：** `boost::lockfree::queue<T>`（`fixed_sized<true>`，编译期默认），构造时分配固定大小的节点池。使用 `std::atomic<size_t>` 跟踪近似大小。

#### UnboundedMpscQueue<T>

```cpp
template<typename T>
class UnboundedMpscQueue {
    explicit UnboundedMpscQueue(size_t initial_capacity = 64);
    bool push(const T&);      // 永不失败（节点池耗尽时自动动态分配）
    bool pop(T&);
    size_t size() const;
    void clear();
};
```

**内部实现：** `boost::lockfree::queue<T, boost::lockfree::fixed_sized<false>>`

**使用场景：** 多生产者线程向单个消费者线程发送工作任务。

---

## 6. Application 框架

**模块路径：** `modules/application/`

Application 框架建立在 `common` 模块的日志系统和 `service` 模块的 IService 接口之上，提供完整的应用生命周期管理。

### 6.1 Application 单例

**文件：** `include/application/application.h` / `src/application.cpp`

#### 功能概述

- 依赖注入容器（泛型服务注册与获取）
- IService 服务生命周期自动管理
- 初始化/启动/停止三阶段回调
- 跨平台信号处理（SIGINT / SIGTERM）
- 优雅关闭（最多等待 10 秒）

#### 生命周期流程

```
Run()
  ├─ 1. 初始化信号处理器
  ├─ 2. 注册 SIGINT/SIGTERM 回调
  ├─ 3. initialize()
  │    ├─ 按顺序 IService::Initialize()
  │    └─ 按顺序 OnInit 回调
  ├─ 4. start()
  │    ├─ 按顺序 IService::Start()
  │    └─ 按顺序 OnStart 回调
  ├─ 5. 主循环（每秒检查 shouldStop）
  └─ 6. gracefulShutdown()
       ├─ running_ = false
       ├─ 等待处理中任务（最长 10 秒）
       └─ stop()
            ├─ 逆序 IService::Stop()
            └─ 逆序 OnStop 回调
```

#### 依赖注入

```cpp
// 泛型服务注册（非 IService）
app.RegisterService<Database>("db", config);

// IService 注册（自动管理生命周期）
app.RegisterService<ZLMService>(zlm_config);
app.RegisterService<HttpClientPoolService>(pool_config);

// 获取服务
auto db = app.GetService<Database>("db");
auto zlm = app.GetService<ZLMService>();
auto svc = app.GetService("zlm_service");  // 返回 shared_ptr<IService>
```

#### 生命周期回调

```cpp
app.OnInit([]() -> bool {   // 初始化
    LOG_MAIN_INFO_AT("Custom init");
    return true;
});
app.OnStart([]() -> bool {  // 启动
    LOG_MAIN_INFO_AT("Custom start");
    return true;
});
app.OnStop([]() {           // 停止
    LOG_MAIN_INFO_AT("Custom stop");
});
```

### 6.2 ServiceContainer

**文件：** `include/application/service_container.h`

独立的服务容器，提供与 Application 类似的服务管理功能，但不需要 Application 的完整生命周期。

```cpp
auto& container = ServiceContainer::getInstance();
container.registerService<MyService>(args...);
container.initializeAll();
container.startAll();
// ...
container.stopAll();
```

### 6.3 SignalHandler

**文件：** `include/application/signal_handler.h` / `src/signal_handler.cpp`

#### 跨平台信号处理

| 信号 | Windows | Linux |
|------|---------|-------|
| SIGINT (Ctrl+C) | `signal(SIGINT, handler)` | `sigaction(SIGINT, &sa, nullptr)` |
| SIGTERM | `signal(SIGTERM, handler)` | `sigaction(SIGTERM, &sa, nullptr)` |
| SIGBREAK/SIGHUP | `signal(SIGBREAK, handler)` | `sigaction(SIGHUP, &sa, nullptr)` |
| SIGUSR1 / SIGUSR2 | ❌ | ✅ |

#### 异步安全设计

信号处理函数必须是异步安全（async-signal-safe）的。`platformSignalHandler()` 只做两件事：

```cpp
static void platformSignalHandler(int signum) {
    instance_->last_signal_.store(signum);
    instance_->stop_requested_.store(true);
    instance_->notifyWaiters();  // 条件变量通知
}
```

在 Linux 上，`write()` 代替 `LOG_MAIN_INFO_AT`（因为日志输出不是异步安全的）。

#### 等待与通知

```cpp
void waitForSignal() {
    unique_lock lock(wait_mutex_);
    wait_cv_.wait(lock, [this]() { return stop_requested_.load(); });
}

void notifyWaiters() {
    wait_cv_.notify_all();
}
```

---

## 7. IService 接口层

**文件：** `modules/service/include/service/iservice.h`

```cpp
class IService {
public:
    virtual ~IService() = default;

    virtual bool Initialize() = 0;               // 初始化（仅一次）
    virtual bool Start() = 0;                     // 启动服务
    virtual void Stop() = 0;                      // 停止服务
    virtual const char* GetName() const = 0;      // 服务唯一名称
    virtual bool IsRunning() const = 0;           // 当前是否运行中
    virtual bool IsInitialized() const = 0;       // 是否已初始化
};
```

**实现示例：**

```cpp
class ZLMService : public IService {
    bool Initialize() override {
        manager_ = std::make_unique<ZLMManager>(config_);
        return manager_->init();
    }
    bool Start() override {
        running_ = true;
        thread_ = std::thread([this]() { manager_->run(); });
        return true;
    }
    void Stop() override {
        running_ = false;
        manager_->stop();
        if (thread_.joinable()) thread_.join();
    }
    const char* GetName() const override { return "zlm_service"; }
    bool IsRunning() const override { return running_; }
    bool IsInitialized() const override { return manager_ != nullptr; }
};
```

---

## 8. 完整数据流示例

### 场景：视频 AI 推理管线

```
  摄像头采集                     文件存储
     │                            ▲
     │ frame                      │ result
     ▼                            │
┌──────────┐   frame   ┌──────────┐   result   ┌──────────┐
│ Decode   │──────────>│ Preprocess│──────────>│  Push    │
│ (cpu exec)│          │ (cpu exec)│           │ (io exec)│
└──────────┘           └──────────┘           └──────────┘
                            │
                            ▼
                       ┌──────────┐
                       │  Infer   │
                       │(inf exec)│
                       └──────────┘
```

### 使用 AsioRuntime 构建

```cpp
#include "common/thread/asio_runtime_framework.h"
#include "common/log/logmanager.h"

struct Frame {
    int64_t seq;            // 帧序号
    std::vector<uint8_t> data;  // 原始数据
    std::vector<float> features; // 推理结果
};

class DecodeNode : public common::thread::INode<Frame> {
    void Process(Frame frame) override {
        LOG_MAIN_INFO_AT("[Decode] seq={}", frame.seq);
        // 模拟解码
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        Emit(std::move(frame));
    }
};

class PreprocessNode : public common::thread::INode<Frame> {
    void Process(Frame frame) override {
        LOG_MAIN_INFO_AT("[Preproc] seq={}", frame.seq);
        // 缩放/归一化
        Emit(std::move(frame));
    }
};

class InferNode : public common::thread::INode<Frame> {
    void Process(Frame frame) override {
        LOG_MAIN_INFO_AT("[Infer] seq={}", frame.seq);
        frame.features = {0.1f, 0.2f, 0.3f}; // 模拟推理
        Emit(std::move(frame));
    }
};

class PushNode : public common::thread::INode<Frame> {
    void Process(Frame frame) override {
        LOG_MAIN_INFO_AT("[Push] seq={} features={}", frame.seq, frame.features.size());
    }
};

int main() {
    LogManager::getInstance().Init("./logs", 1);

    // 构建 Asio 运行时管线
    common::thread::asio::AsioRuntime<Frame> rt;
    rt.AddDefaultExecutors(4);  // cpu(4线程), inference(1线程), io(1线程)

    rt.AddNode("decode", std::make_shared<DecodeNode>(),
               {.executor_name = "cpu", .mailbox_capacity = 128,
                .backpressure = BackpressurePolicy::DropOldest});
    rt.AddNode("preproc", std::make_shared<PreprocessNode>(),
               {.executor_name = "cpu", .mailbox_capacity = 256});
    rt.AddNode("infer", std::make_shared<InferNode>(),
               {.executor_name = "inference", .mailbox_capacity = 16});
    rt.AddNode("push", std::make_shared<PushNode>(),
               {.executor_name = "io", .mailbox_capacity = 1024});

    rt.Connect("decode", "preproc");
    rt.Connect("preproc", "infer");
    rt.Connect("infer", "push");

    rt.SetErrorHandler([](const std::string& node_id, std::exception_ptr eptr) {
        try { std::rethrow_exception(eptr); }
        catch (const std::exception& e) {
            LOG_MAIN_ERROR_AT("Node '{}' error: {}", node_id, e.what());
        }
    });

    rt.Start();

    // 外部注入 100 帧
    for (int64_t i = 0; i < 100; ++i) {
        rt.Push("decode", Frame{i, std::vector<uint8_t>(1024)});
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));
    rt.Stop();

    // 输出统计
    NodeMetricsSnapshot metrics;
    rt.GetMetrics("decode", metrics);
    LOG_MAIN_INFO_AT("Decode: enqueued={}, processed={}, dropped={}",
                     metrics.enqueued, metrics.processed, metrics.dropped);

    return 0;
}
```

---

## 9. 依赖与构建集成

### CMakeLists.txt（`modules/common/CMakeLists.txt`）

```cmake
project(common_lib LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)

# 收集头文件与源文件
file(GLOB_RECURSE LIB_SOURCES
    "src/*.cpp"
    "include/common/log/*.h"
    "include/common/config/*.h"
    "include/common/pool/*.hpp"
    "include/common/thread/*.h"
)

add_library(${PROJECT_NAME} ${LIB_SOURCES})

target_include_directories(${PROJECT_NAME}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        spdlog::spdlog         # 日志
        fmt::fmt               # 格式化
        yaml-cpp::yaml-cpp     # 配置解析
        Boost::json            # JSON 处理
        Boost::system          # Asio 所需
        Boost::filesystem      # 文件路径
        Boost::process         # 进程管理
        Boost::lockfree        # 无锁队列
        Boost::asio            # Asio 运行时
)
```

### 依赖关系图

```
┌─────────────────────────────┐
│        Application          │  modules/application/
│  (注入容器 + 生命周期 + 信号) │
└──────────┬──────────────────┘
           │ depends on
           ▼
┌─────────────────────────────┐
│     application_lib         │
└──────────┬──────────────────┘
           │ depends on
           ▼
┌─────────────────────────────┐     ┌──────────────────────────┐
│       service_lib           │     │       common_lib          │
│ (IService 接口头文件)         │◄────│ (日志 + 配置 + 线程运行时  │
│                             │     │  + 无锁队列 + 对象池)     │
└─────────────────────────────┘     └──────────────────────────┘
                                            │
                                   ┌────────┼────────┬──────────┐
                                   ▼        ▼        ▼          ▼
                              spdlog    yaml-cpp   Boost::asio  Boost::lockfree
```

### 外部依赖

| 依赖 | 用途 | 提供者 |
|------|------|--------|
| `spdlog` | 异步日志框架 | vcpkg `spdlog` |
| `fmt` | 格式化字符串 | vcpkg `fmt` |
| `yaml-cpp` | YAML 配置解析 | vcpkg `yaml-cpp` |
| `Boost::asio` | 异步 IO / io_context | vcpkg `boost-asio` |
| `Boost::lockfree` | 无锁队列 SPSC/MPSC | vcpkg `boost-lockfree` |
| `Boost::system` | Asio 错误码 | vcpkg `boost-system` |
| `Boost::filesystem` | 日志文件路径管理 | vcpkg `boost-filesystem` |
| `Boost::process` | 外部进程管理 | vcpkg `boost-process` |
| `Boost::json` | JSON 序列化 | vcpkg `boost-json` |

---

## 附录 A：指标监控

通过 `Runtime::GetMetrics(node_id, out)` 获取快照：

```cpp
struct NodeMetricsSnapshot {
    uint64_t enqueued;    // 累计入队数
    uint64_t processed;   // 累计处理成功数
    uint64_t dropped;     // 累计丢弃数
    uint64_t rejected;    // 累计拒绝数（Mailbox 关闭后）
    uint64_t errors;      // 累计异常数
};
```

**性能评估参考：**

| 场景 | 入队速率 | 说明 |
|------|----------|------|
| processed ≈ enqueued | 稳定 | 节点处理能力充足 |
| dropped > 0 | 过载 | 背压生效，需要扩容或优化 |
| rejected > 0 | 异常 | Mailbox 已关闭，检查生命周期 |
| errors > 0 | 异常 | 节点内部有未处理的异常 |

---

## 附录 B：常见问题

**Q1：什么时候用 Teaching Runtime，什么时候用 Asio Runtime？**

> **Asio Runtime** 是生产首选。Teaching Runtime 适用于教学演示或不需要与 Asio 其他组件集成的简单场景。

**Q2：如何选择合适的背压策略？**

> - **实时显示管线**：`DropNewest` — 显示最新帧即可，丢弃旧的
> - **AI 推理管线**：`DropOldest` — 优先处理最新输入，避免堆积延迟
> - **关键数据存储**：`Block` 或 `Unbounded` — 不允许丢帧
> - **日志写入**：`Unbounded`（加环形缓冲区上限）

**Q3：如何处理节点异常？**

> 通过 `Runtime::SetErrorHandler()` 注册全局错误处理回调。节点内部的异常会被 Scheduler 捕获，不会导致线程或进程崩溃。

**Q4：如何保证节点线程安全？**

> Teaching Runtime 中节点需要自己加锁保护共享状态。Asio Runtime 中可以使用 `strand` 保证同一节点串行执行。通常建议节点设计为无状态的纯函数式处理逻辑。
