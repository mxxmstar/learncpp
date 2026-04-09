"""
Python gRPC 客户端模块
用于与 C++ gRPC 服务端通信
"""

import grpc
from typing import Optional, Callable
import hello_pb2
import hello_pb2_grpc


class HelloGrpcClient:
    """Hello gRPC 客户端"""
    
    def __init__(self, target: str = "localhost:50051"):
        """
        初始化客户端
        
        Args:
            target: gRPC 服务器地址，例如 "localhost:50051"
        """
        self.target = target
        self.channel = None
        self.stub = None
    
    def connect(self) -> bool:
        """
        连接到 gRPC 服务器
        
        Returns:
            bool: 连接成功返回 True
        """
        try:
            # 创建不安全的 channel（与 C++ 服务端对应）
            self.channel = grpc.insecure_channel(self.target)
            
            # 创建 stub
            self.stub = hello_pb2_grpc.HelloServiceStub(self.channel)
            
            # 测试连接
            grpc.channel_ready_future(self.channel).result(timeout=5)
            print(f"[HelloGrpcClient] Connected to {self.target}")
            return True
            
        except grpc.FutureTimeoutError:
            print(f"[HelloGrpcClient] Connection timeout to {self.target}")
            self.close()
            return False
        except Exception as e:
            print(f"[HelloGrpcClient] Connection failed: {e}")
            self.close()
            return False
    
    def say_hello(self, name: str, timeout: int = 5) -> Optional[str]:
        """
        Unary RPC - 简单请求响应
        
        Args:
            name: 用户名
            timeout: 超时时间（秒）
            
        Returns:
            str: 服务器的响应消息，失败返回 None
        """
        if not self.stub:
            print("[HelloGrpcClient] Not connected")
            return None
        
        try:
            # 创建请求
            request = hello_pb2.HelloRequest(name=name, count=1)
            
            # 调用 RPC
            response = self.stub.SayHello(request, timeout=timeout)
            
            print(f"[HelloGrpcClient] SayHello response: {response.message}")
            return response.message
            
        except grpc.RpcError as e:
            print(f"[HelloGrpcClient] SayHello failed: {e.code()}: {e.details()}")
            return None
    
    def say_hello_stream(
        self,
        name: str,
        count: int = 5,
        callback: Optional[Callable[[str], None]] = None,
        timeout: int = 10
    ) -> bool:
        """
        Server Streaming RPC - 服务端流式响应
        
        Args:
            name: 用户名
            count: 请求的消息数量
            callback: 每条消息的回调函数
            timeout: 超时时间（秒）
            
        Returns:
            bool: 成功返回 True
        """
        if not self.stub:
            print("[HelloGrpcClient] Not connected")
            return False
        
        try:
            # 创建请求
            request = hello_pb2.HelloRequest(name=name, count=count)
            
            # 调用流式 RPC
            responses = self.stub.SayHelloStream(request, timeout=timeout)
            
            # 处理响应流
            for response in responses:
                message = response.message
                print(f"[HelloGrpcClient] Stream message: {message}")
                
                if callback:
                    callback(message)
            
            return True
            
        except grpc.RpcError as e:
            print(f"[HelloGrpcClient] SayHelloStream failed: {e.code()}: {e.details()}")
            return False
    
    def close(self):
        """关闭连接"""
        if self.channel:
            self.channel.close()
            self.channel = None
            self.stub = None
            print("[HelloGrpcClient] Connection closed")
    
    def __enter__(self):
        """上下文管理器入口"""
        self.connect()
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        """上下文管理器出口"""
        self.close()
        return False
