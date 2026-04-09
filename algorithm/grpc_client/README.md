# Python gRPC 客户端

Python gRPC 客户端模块，用于与 C++ gRPC 服务端通信。

## 目录结构

```
algorithm/grpc_client/
├── __init__.py              # 模块初始化
├── hello_client.py          # Hello gRPC 客户端实现
├── generate_proto.py        # 生成 Python gRPC 代码脚本
├── requirements.txt         # Python 依赖
├── hello_pb2.py            # 生成的 protobuf 代码（自动生成）
├── hello_pb2_grpc.py       # 生成的 gRPC 代码（自动生成）
└── README.md               # 使用说明
```

## 安装依赖

```bash
cd algorithm/grpc_client
pip install -r requirements.txt
```

## 生成 gRPC 代码

在运行客户端之前，需要先从 proto 文件生成 Python 代码：

```bash
cd algorithm/grpc_client
python generate_proto.py
```

这会生成两个文件：
- `hello_pb2.py` - Protobuf 消息定义
- `hello_pb2_grpc.py` - gRPC 服务定义

## 使用方法

### 1. 基本使用

```python
from grpc_client import HelloGrpcClient

# 创建客户端
client = HelloGrpcClient("localhost:50051")

# 连接服务器
if client.connect():
    # 调用 Unary RPC
    response = client.say_hello("World")
    print(f"Response: {response}")
    
    # 关闭连接
    client.close()
```

### 2. 使用上下文管理器

```python
from grpc_client import HelloGrpcClient

with HelloGrpcClient("localhost:50051") as client:
    response = client.say_hello("World")
    print(f"Response: {response}")
# 自动关闭连接
```

### 3. 流式 RPC

```python
from grpc_client import HelloGrpcClient

def on_message(msg):
    print(f"Received: {msg}")

with HelloGrpcClient("localhost:50051") as client:
    success = client.say_hello_stream(
        name="StreamTest",
        count=5,
        callback=on_message,
        timeout=10
    )
```

## API 参考

### HelloGrpcClient

#### 构造函数
```python
HelloGrpcClient(target: str = "localhost:50051")
```

**参数:**
- `target`: gRPC 服务器地址

#### connect() -> bool
连接到 gRPC 服务器

**返回:** 连接成功返回 True

#### say_hello(name: str, timeout: int = 5) -> Optional[str]
Unary RPC - 简单请求响应

**参数:**
- `name`: 用户名
- `timeout`: 超时时间（秒）

**返回:** 服务器响应消息，失败返回 None

#### say_hello_stream(name: str, count: int = 5, callback: Callable, timeout: int = 10) -> bool
Server Streaming RPC - 服务端流式响应

**参数:**
- `name`: 用户名
- `count`: 请求的消息数量
- `callback`: 每条消息的回调函数
- `timeout`: 超时时间（秒）

**返回:** 成功返回 True

#### close()
关闭连接

## 运行测试

首先确保 C++ gRPC 服务器正在运行：

```bash
# 在一个终端中启动 C++ 服务器
cd out/build/x64-Debug
./bin/test_grpc_hello.exe
```

然后在另一个终端运行 Python 测试：

```bash
cd test/grpc
python test_grpc_python.py
```

## 示例输出

```
############################################################
# Python gRPC Client Tests
# Testing communication with C++ gRPC Server
############################################################

============================================================
Test 1: Unary RPC - SayHello
============================================================
[HelloGrpcClient] Connected to localhost:50051

--- Test 1.1: Basic SayHello ---
[HelloGrpcClient] SayHello response: Hello, World!
✓ Response: Hello, World!

--- Test 1.2: Different Names ---
[HelloGrpcClient] SayHello response: Hello, Alice!
✓ Alice: Hello, Alice!
[HelloGrpcClient] SayHello response: Hello, Bob!
✓ Bob: Hello, Bob!
[HelloGrpcClient] SayHello response: Hello, Charlie!
✓ Charlie: Hello, Charlie!

✓ Unary RPC tests passed!

============================================================
Test Summary
============================================================
Unary RPC                      ✓ PASSED
Streaming RPC                  ✓ PASSED
Context Manager                ✓ PASSED
Connection Failure             ✓ PASSED
------------------------------------------------------------
Total: 4/4 tests passed

🎉 All tests passed!
```

## 注意事项

1. **编码问题**: 确保 proto 文件使用 UTF-8 编码
2. **服务器地址**: 默认连接到 `localhost:50051`，可以根据需要修改
3. **超时设置**: 建议为所有 RPC 调用设置合理的超时时间
4. **错误处理**: 所有方法都有完善的错误处理，失败时返回 None 或 False
