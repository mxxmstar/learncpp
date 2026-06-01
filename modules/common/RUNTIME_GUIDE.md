# Common Thread Runtime Guide

本文档说明 `modules/common/include/common/runtime/__example/runtime_framework.h` 中的教学版线程运行时。它根据“Node + Mailbox + Executor + Scheduler”的模型拆分职责，便于理解实时 AI Runtime 中线程和节点的关系。

> 注意：该实现会继续保留，适合作为学习和对照版本。项目实际推荐优先使用 Asio 版 `common/runtime/asio/asio_runtime_framework.h`。

## 核心原则

Node 不等于线程。Node 只处理数据，不创建线程、不管理线程、不写 `while(true)` 调度循环。

Runtime 负责连接 graph，Scheduler 负责调度，Executor 负责真实线程，Mailbox 负责节点输入队列和背压策略。

## 文件结构

| 文件 | 作用 |
| --- | --- |
| `node.h` | 定义 `INode<Frame>` 和 `ISourceNode<Frame>` |
| `mailbox.h` | 定义 `IMailBox`、`SPSCMailBox`、`MPMCMailBox` 和背压策略 |
| `executor.h` | 定义线程池执行器，以及 `IExecutorTaskQueue` / `MpscExecutorTaskQueue` |
| `scheduler.h` | 把 mailbox 中的数据派发到 executor |
| `runtime.h` | 管理 node/source/executor/graph 生命周期 |
| `runtime_framework.h` | 聚合 include |

## Node

Passive Node 只实现 `Process(frame)`：

```cpp
class MyNode : public common::runtime::INode<FramePtr> {
public:
    void Process(FramePtr frame) override {
        // 只处理数据，不管理线程
        Emit(std::move(frame));
    }
};
```

Active Source Node 自己产生数据，因此有 `Start()` 和 `Stop()`：

```cpp
class CameraSource : public common::runtime::ISourceNode<FramePtr> {
public:
    void Start() override {
        Emit(frame);
    }

    void Stop() override {}
};
```

## Mailbox 和背压

每个 passive node 都有一个 mailbox。上游发来的 frame 先进入 mailbox，再由 scheduler 派发。

当前项目中的 video pipeline 场景优先使用 SPSC：一个上游生产者对应一个下游消费者。因此 runtime 默认创建 `SPSCMailBox<T>`，底层使用 `BoundedSpscQueue<T>` 或 `UnboundedSpscQueue<T>`。

`MPMCMailBox<T>` 是原先通用 mailbox 的保留版本，只在确实存在多个生产者和多个消费者同时访问同一 mailbox 时使用。普通节点链路不要默认使用 MPMC。

支持的策略：

| 策略 | 行为 | 适合场景 |
| --- | --- | --- |
| `Block` | mailbox 满时阻塞生产者 | 不能丢数据的控制消息 |
| `DropNewest` | mailbox 满时丢弃新 frame | 保留旧数据完整性 |
| `DropOldest` | mailbox 满时丢弃最旧 frame | 实时视频，优先处理新帧 |
| `Unbounded` | 不限制队列长度 | 少量消息或上层已限流 |

实时视频链路通常优先使用 `DropOldest`，避免处理过期帧。

## Executor

Executor 是真正拥有线程的对象。教学版线程池内部不再使用一个共享 `deque`，而是为每个 worker 分配一个 `MpscExecutorTaskQueue`：多个调用方可以投递任务，但每个 worker 只消费自己的队列。

| Executor | 用途 |
| --- | --- |
| `SingleThreadExecutor` | 一个线程串行处理多个轻量 node |
| `ThreadPoolExecutor` | CPU 轻量任务共享线程池 |
| `InferenceExecutor` | GPU 推理类任务的专用执行器 |
| `IOExecutor` | puller/pusher/编码推流等阻塞任务 |

教学版 executor 内部用 `std::thread + condition_variable + task queue` 实现，目的是把调度模型讲清楚。

## Scheduler

Scheduler 的职责是：

- 根据 node 的 mailbox 状态决定是否调度
- 把 node 的 `Process(frame)` 投递到指定 executor
- 用 `scheduled` 标记避免同一个 node 被重复并发 drain
- 统计 `enqueued / processed / dropped / rejected / errors`

默认 drain 会按 `max_batch_size` 分批处理，避免单个 node 长时间占住 executor。

## Runtime

Runtime 负责：

- `AddExecutor(...)`
- `AddNode(...)`
- `AddSource(...)`
- `Connect(from, to)`
- `Start()`
- `Stop()`
- `Push(node_id, frame)`

示例：

```cpp
using Runtime = common::runtime::Runtime<FramePtr>;

Runtime runtime;
runtime.AddDefaultExecutors();

common::runtime::NodeOptions options;
options.executor_name = "cpu";
options.backpressure = common::runtime::BackpressurePolicy::DropOldest;
options.mailbox_capacity = 8;

runtime.AddSource("camera", camera_source);
runtime.AddNode("preprocess", preprocess_node, options);
runtime.AddNode("infer", infer_node, {
    .executor_name = "inference",
    .mailbox_capacity = 2,
    .max_batch_size = 1,
    .backpressure = common::runtime::BackpressurePolicy::DropOldest,
});

runtime.Connect("camera", "preprocess");
runtime.Connect("preprocess", "infer");
runtime.Start();
```

## Frame 类型建议

`Frame` 建议使用轻量可拷贝句柄，例如：

```cpp
using FramePtr = std::shared_ptr<MediaFrame>;
```

Runtime 在 fan-out 时会把同一个 frame 分发给多个下游。对于视频帧、packet、推理结果，使用 `shared_ptr` 通常比拷贝大对象更合适。

## 与 Asio 版的区别

教学版重点是拆清楚概念，因此自己实现了线程池。Asio 版会把执行队列、事件循环、strand 串行化交给 Boost.Asio，更符合当前项目的技术底座。

实际项目中建议：

- 学习模型：看 `runtime_framework.h`
- 项目使用：看 `asio_runtime_framework.h`
- 网络、timer、异步 IO：优先放进 Asio 版 runtime
