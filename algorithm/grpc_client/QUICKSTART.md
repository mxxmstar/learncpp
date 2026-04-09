# Python gRPC 快速开始

## 1. 安装 Python 依赖

```bash
cd algorithm/grpc_client
pip install -r requirements.txt
```

## 2. 生成 gRPC 代码

```bash
python generate_proto.py
```

这会生成：
- `hello_pb2.py`
- `hello_pb2_grpc.py`

## 3. 启动 C++ gRPC 服务器

在一个终端中：

```bash
cd out/build/x64-Debug
.\bin\test_grpc_hello.exe
```

选择测试选项启动服务器（例如选项 1）。

## 4. 运行 Python 测试

在另一个终端中：

```bash
cd test/grpc
python test_grpc_python.py
```

## 5. 在自己的代码中使用

```python
import sys
sys.path.insert(0, 'algorithm/grpc_client')

from grpc_client import HelloGrpcClient

with HelloGrpcClient("localhost:50051") as client:
    # Unary RPC
    response = client.say_hello("World")
    print(f"Response: {response}")
    
    # Streaming RPC
    def on_message(msg):
        print(f"Stream: {msg}")
    
    client.say_hello_stream("Stream", count=3, callback=on_message)
```

## 常见问题

### Q: 找不到模块
A: 确保已经生成了 gRPC 代码（运行 `generate_proto.py`）

### Q: 连接失败
A: 确保 C++ 服务器正在运行，并且地址正确（默认 `localhost:50051`）

### Q: 缺少依赖
A: 运行 `pip install -r requirements.txt` 安装所有依赖
