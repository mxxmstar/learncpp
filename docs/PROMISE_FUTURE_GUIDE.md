# std::promise 和 std::future 使用指南

## 📚 目录

1. [基本概念](#基本概念)
2. [核心组件](#核心组件)
3. [基本用法](#基本用法)
4. [高级用法](#高级用法)
5. [常见场景](#常见场景)
6. [注意事项](#注意事项)
7. [实战示例](#实战示例)

---

## 基本概念

### 什么是 Promise/Future？

**Promise/Future** 是 C++11 引入的异步编程机制，用于在**不同线程间传递值或异常**。

- **`std::promise<T>`**：承诺者，负责**设置**值
- **`std::future<T>`**：期待者，负责**获取**值

### 工作原理

```
线程 A (生产者)                    线程 B (消费者)
    |                                    |
    |  std::promise<int> p;              |
    |  std::future<int> f = p.get_future();
    |                                    |
    |  p.set_value(42);  ──────────────► |  f.get() → 42
    |                                    |
```

---

## 核心组件

### 1. std::promise

```cpp
template<class T> class promise;
```

**主要方法**：

| 方法 | 说明 |
|------|------|
| `get_future()` | 获取关联的 future 对象 |
| `set_value(val)` | 设置值（只能调用一次） |
| `set_exception(e)` | 设置异常 |
| `set_value_at_thread_exit(val)` | 线程退出时设置值 |

### 2. std::future

```cpp
template<class T> class future;
```

**主要方法**：

| 方法 | 说明 |
|------|------|
| `get()` | 获取值（阻塞直到值就绪） |
| `wait()` | 等待值就绪（不获取） |
| `wait_for(duration)` | 限时等待 |
| `wait_until(timepoint)` | 等待到指定时间点 |
| `valid()` | 检查 future 是否有效 |

### 3. std::shared_future

与 `future` 类似，但可以被**多个线程共享**（可拷贝）。

---

## 基本用法

### 示例 1：基础用法

```cpp
#include <iostream>
#include <thread>
#include <future>

void producer(std::promise<int>& prom) {
    // 模拟耗时操作
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // 设置值
    prom.set_value(42);
}

int main() {
    // 1. 创建 promise
    std::promise<int> prom;
    
    // 2. 获取 future
    std::future<int> fut = prom.get_future();
    
    // 3. 启动生产者线程
    std::thread t(producer, std::ref(prom));
    
    // 4. 等待并获取结果（阻塞）
    std::cout << "Waiting for result..." << std::endl;
    int result = fut.get();  // 阻塞直到 set_value 被调用
    
    std::cout << "Result: " << result << std::endl;
    
    t.join();
    return 0;
}
```

**输出**：
```
Waiting for result...
(Result appears after 2 seconds)
Result: 42
```

### 示例 2：传递异常

```cpp
void risky_operation(std::promise<int>& prom) {
    try {
        // 可能抛出异常的操作
        throw std::runtime_error("Something went wrong!");
        prom.set_value(42);  // 不会执行
    } catch (...) {
        // 捕获并传递异常
        prom.set_exception(std::current_exception());
    }
}

int main() {
    std::promise<int> prom;
    std::future<int> fut = prom.get_future();
    
    std::thread t(risky_operation, std::ref(prom));
    
    try {
        int result = fut.get();  // 会抛出异常
    } catch (const std::exception& e) {
        std::cout << "Caught exception: " << e.what() << std::endl;
    }
    
    t.join();
    return 0;
}
```

**输出**：
```
Caught exception: Something went wrong!
```

---

## 高级用法

### 1. 限时等待（wait_for）

```cpp
std::promise<int> prom;
std::future<int> fut = prom.get_future();

// 启动异步任务
std::thread t([&prom]() {
    std::this_thread::sleep_for(std::chrono::seconds(5));
    prom.set_value(42);
});

// 最多等待 2 秒
auto status = fut.wait_for(std::chrono::seconds(2));

if (status == std::future_status::ready) {
    std::cout << "Result: " << fut.get() << std::endl;
} else if (status == std::future_status::timeout) {
    std::cout << "Timeout! Result not ready yet." << std::endl;
} else {
    std::cout << "Deferred (shouldn't happen with threads)" << std::endl;
}

t.join();
```

**输出**：
```
Timeout! Result not ready yet.
```

### 2. 多值传递（使用 pair/tuple）

```cpp
// 传递多个值
std::promise<std::pair<bool, std::string>> prom;
std::future<std::pair<bool, std::string>> fut = prom.get_future();

std::thread t([&prom]() {
    // 模拟 HTTP 请求
    bool success = true;
    std::string response = "{\"code\": 0, \"msg\": \"OK\"}";
    
    prom.set_value({success, response});
});

auto [success, response] = fut.get();
std::cout << "Success: " << success << ", Response: " << response << std::endl;

t.join();
```

### 3. 共享 Future（shared_future）

```cpp
std::promise<int> prom;
std::shared_future<int> shared_fut = prom.get_future().share();

// 多个线程可以等待同一个结果
std::thread t1([shared_fut]() {
    std::cout << "Thread 1 got: " << shared_fut.get() << std::endl;
});

std::thread t2([shared_fut]() {
    std::cout << "Thread 2 got: " << shared_fut.get() << std::endl;
});

std::this_thread::sleep_for(std::chrono::seconds(1));
prom.set_value(42);

t1.join();
t2.join();
```

**输出**：
```
Thread 1 got: 42
Thread 2 got: 42
```

---

## 常见场景

### 场景 1：异步 HTTP 请求（本项目用例）

```cpp
// API Handler 中等待异步 HTTP 请求完成
void handleApiRequest(json::object& rsp) {
    std::promise<std::pair<bool, json::object>> promise;
    auto future = promise.get_future();
    
    // 发起异步请求
    httpClient.GetAsync("/api/data", [&promise](bool success, json::object response) {
        promise.set_value({success, response});
    });
    
    // 等待结果（最多 10 秒）
    auto status = future.wait_for(std::chrono::seconds(10));
    
    if (status == std::future_status::timeout) {
        rsp["code"] = 504;
        rsp["msg"] = "Request timeout";
        return;
    }
    
    auto [success, response] = future.get();
    
    if (!success) {
        rsp["code"] = 502;
        rsp["msg"] = "HTTP request failed";
        return;
    }
    
    // 处理成功响应
    rsp["code"] = 200;
    rsp["data"] = response;
}
```

### 场景 2：并行计算

```cpp
std::vector<std::future<int>> futures;

for (int i = 0; i < 4; ++i) {
    std::promise<int> prom;
    futures.push_back(prom.get_future());
    
    std::thread t([prom = std::move(prom), i]() mutable {
        // 模拟耗时计算
        int result = i * i;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        prom.set_value(result);
    });
    
    t.detach();  // 让线程独立运行
}

// 收集所有结果
for (auto& fut : futures) {
    std::cout << "Result: " << fut.get() << std::endl;
}
```

**输出**（约 1 秒后）：
```
Result: 0
Result: 1
Result: 4
Result: 9
```

### 场景 3：任务队列

```cpp
class TaskQueue {
public:
    template<typename Func>
    auto submit(Func func) -> std::future<decltype(func())> {
        using ReturnType = decltype(func());
        
        std::promise<ReturnType> prom;
        std::future<ReturnType> fut = prom.get_future();
        
        // 将任务和 promise 打包提交到线程池
        thread_pool_.enqueue([prom = std::move(prom), func]() mutable {
            try {
                prom.set_value(func());
            } catch (...) {
                prom.set_exception(std::current_exception());
            }
        });
        
        return fut;
    }
    
private:
    ThreadPool thread_pool_;
};

// 使用
TaskQueue queue;
auto future = queue.submit([]() {
    return 42;
});

std::cout << "Result: " << future.get() << std::endl;
```

---

## 注意事项

### ⚠️ 1. promise 只能设置一次值

```cpp
std::promise<int> prom;
prom.set_value(42);
prom.set_value(100);  // ❌ 抛出 std::future_error 异常！
```

### ⚠️ 2. future::get() 只能调用一次

```cpp
std::promise<int> prom;
std::future<int> fut = prom.get_future();
prom.set_value(42);

int val1 = fut.get();  // ✅ 正常
int val2 = fut.get();  // ❌ 行为未定义（通常抛出异常）
```

如果需要多次获取，使用 `std::shared_future`。

### ⚠️ 3. 忘记设置值会导致死锁

```cpp
std::promise<int> prom;
std::future<int> fut = prom.get_future();

// 如果线程中没有调用 set_value，fut.get() 会永久阻塞！
std::thread t([]() {
    // 忘记调用 prom.set_value()
});

fut.get();  // ❌ 永久阻塞
```

**解决方案**：确保所有路径都设置值或异常。

### ⚠️ 4. promise 和 future 的生命周期

```cpp
std::future<int> createFuture() {
    std::promise<int> prom;  // 局部变量
    std::future<int> fut = prom.get_future();
    
    std::thread t([prom = std::move(prom)]() mutable {
        prom.set_value(42);
    });
    
    t.detach();
    return fut;  // ✅ future 可以安全返回
}
```

### ⚠️ 5. 异常安全

```cpp
std::promise<int> prom;
std::future<int> fut = prom.get_future();

try {
    std::thread t([&prom]() {
        throw std::runtime_error("Error");
        // prom.set_value() 不会被调用
    });
    
    int val = fut.get();  // ❌ 会抛出异常，但不是我们设置的
    t.join();
} catch (...) {
    // 需要正确处理
}
```

**最佳实践**：在线程中使用 try-catch 并通过 `set_exception` 传递异常。

---

## 实战示例

### 完整示例：异步数据库查询

```cpp
#include <iostream>
#include <thread>
#include <future>
#include <string>
#include <chrono>

struct QueryResult {
    bool success;
    std::string data;
    int row_count;
};

// 模拟数据库查询
QueryResult simulateDbQuery(const std::string& sql) {
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    if (sql.find("ERROR") != std::string::npos) {
        throw std::runtime_error("SQL syntax error");
    }
    
    return {true, "User data", 10};
}

// 异步查询函数
std::future<QueryResult> asyncQuery(const std::string& sql) {
    std::promise<QueryResult> prom;
    std::future<QueryResult> fut = prom.get_future();
    
    std::thread t([prom = std::move(prom), sql]() mutable {
        try {
            QueryResult result = simulateDbQuery(sql);
            prom.set_value(result);
        } catch (...) {
            prom.set_exception(std::current_exception());
        }
    });
    
    t.detach();
    return fut;
}

int main() {
    // 查询 1：成功
    auto fut1 = asyncQuery("SELECT * FROM users");
    
    // 查询 2：失败
    auto fut2 = asyncQuery("SELECT ERROR FROM users");
    
    try {
        auto result1 = fut1.get();
        std::cout << "Query 1 success: " << result1.data 
                  << ", rows: " << result1.row_count << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Query 1 failed: " << e.what() << std::endl;
    }
    
    try {
        auto result2 = fut2.get();
        std::cout << "Query 2 success: " << result2.data << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Query 2 failed: " << e.what() << std::endl;
    }
    
    return 0;
}
```

**输出**：
```
Query 1 success: User data, rows: 10
Query 2 failed: SQL syntax error
```

---

## 与其他异步机制对比

| 特性 | Promise/Future | Callback | std::async |
|------|---------------|----------|------------|
| **易用性** | ⭐⭐⭐ | ⭐⭐ | ⭐⭐⭐⭐ |
| **灵活性** | ⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐ |
| **错误处理** | ✅ 优秀 | ⚠️ 需手动处理 | ✅ 良好 |
| **组合能力** | ✅ 良好 | ⚠️ 回调地狱 | ❌ 较差 |
| **性能开销** | 中等 | 低 | 较高 |
| **适用场景** | 一次性异步任务 | 事件驱动 | 简单并行计算 |

---

## 总结

### ✅ 何时使用 Promise/Future？

1. **需要将异步结果同步化**（如 HTTP 请求）
2. **需要在不同线程间传递值**
3. **需要统一的错误处理机制**
4. **需要限时等待**

### ❌ 何时不使用？

1. **高频小任务**（考虑使用回调或协程）
2. **事件驱动模型**（考虑使用信号槽）
3. **简单并行计算**（考虑使用 `std::async`）

### 💡 最佳实践

1. **始终处理超时**：使用 `wait_for` 避免永久阻塞
2. **传递异常**：使用 `set_exception` 而非忽略错误
3. **RAII 管理**：确保 promise 在所有路径都被设置
4. **文档化**：明确说明 future 的预期行为和超时时间

---

## 参考资料

- [C++ Reference: std::promise](https://en.cppreference.com/w/cpp/thread/promise)
- [C++ Reference: std::future](https://en.cppreference.com/w/cpp/thread/future)
- [C++ Concurrency in Action](https://www.manning.com/books/c-plus-plus-concurrency-in-action-second-edition)

---

**最后更新**: 2026-04-27  
**作者**: Lingma AI Assistant
