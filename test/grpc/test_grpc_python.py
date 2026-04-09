"""
Python gRPC 客户端测试
测试与 C++ gRPC 服务端的通信
"""

import sys
import os
import time

# 添加 grpc_client 模块路径
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'algorithm', 'grpc_client'))

from hello_client import HelloGrpcClient


def test_unary_rpc():
    """测试 Unary RPC - SayHello"""
    print("\n" + "="*60)
    print("Test 1: Unary RPC - SayHello")
    print("="*60)
    
    client = HelloGrpcClient("localhost:50051")
    
    if not client.connect():
        print("False Failed to connect to server")
        return False
    
    try:
        # 测试 1: 基本调用
        print("\n--- Test 1.1: Basic SayHello ---")
        response = client.say_hello("World")
        if response:
            print(f"Success Response: {response}")
        else:
            print("False No response received")
            return False
        
        # 测试 2: 不同的名字
        print("\n--- Test 1.2: Different Names ---")
        names = ["Alice", "Bob", "Charlie"]
        for name in names:
            response = client.say_hello(name)
            if response:
                print(f"Success {name}: {response}")
            else:
                print(f"False {name}: No response")
        
        # 测试 3: 超时测试
        print("\n--- Test 1.3: Timeout Test ---")
        response = client.say_hello("TimeoutTest", timeout=1)
        if response:
            print(f"Success Response with short timeout: {response}")
        
        print("\nSuccess Unary RPC tests passed!")
        return True
        
    except Exception as e:
        print(f"\nFalse Test failed with exception: {e}")
        return False
    finally:
        client.close()


def test_streaming_rpc():
    """测试 Server Streaming RPC - SayHelloStream"""
    print("\n" + "="*60)
    print("Test 2: Server Streaming RPC - SayHelloStream")
    print("="*60)
    
    client = HelloGrpcClient("localhost:50051")
    
    if not client.connect():
        print("False Failed to connect to server")
        return False
    
    try:
        # 测试 1: 基本流式调用
        print("\n--- Test 2.1: Basic Streaming ---")
        messages_received = []
        
        def on_message(msg):
            messages_received.append(msg)
        
        success = client.say_hello_stream(
            name="StreamTest",
            count=5,
            callback=on_message,
            timeout=10
        )
        
        if success and len(messages_received) == 5:
            print(f"Success Received {len(messages_received)} messages")
            for i, msg in enumerate(messages_received, 1):
                print(f"  {i}. {msg}")
        else:
            print(f"False Expected 5 messages, got {len(messages_received)}")
            return False
        
        # 测试 2: 不同数量的消息
        print("\n--- Test 2.2: Different Counts ---")
        for count in [1, 3, 10]:
            messages = []
            success = client.say_hello_stream(
                name=f"Count{count}",
                count=count,
                callback=lambda msg, m=messages: m.append(msg),
                timeout=15
            )
            
            if success and len(messages) == count:
                print(f"Success Count {count}: Received {len(messages)} messages")
            else:
                print(f"False Count {count}: Expected {count}, got {len(messages)}")
                return False
        
        print("\nSuccess Streaming RPC tests passed!")
        return True
        
    except Exception as e:
        print(f"\nFalse Test failed with exception: {e}")
        import traceback
        traceback.print_exc()
        return False
    finally:
        client.close()


def test_context_manager():
    """测试上下文管理器"""
    print("\n" + "="*60)
    print("Test 3: Context Manager")
    print("="*60)
    
    try:
        with HelloGrpcClient("localhost:50051") as client:
            response = client.say_hello("ContextManager")
            if response:
                print(f"Success Context manager test passed: {response}")
                return True
            else:
                print("False No response in context manager")
                return False
    except Exception as e:
        print(f"False Context manager test failed: {e}")
        return False


def test_connection_failure():
    """测试连接失败处理"""
    print("\n" + "="*60)
    print("Test 4: Connection Failure Handling")
    print("="*60)
    
    # 尝试连接到不存在的服务器
    client = HelloGrpcClient("localhost:99999")
    
    if not client.connect():
        print("Success Correctly handled connection failure")
        return True
    else:
        print("False Should have failed to connect")
        client.close()
        return False


def main():
    """运行所有测试"""
    print("\n" + "#"*60)
    print("# Python gRPC Client Tests")
    print("# Testing communication with C++ gRPC Server")
    print("#"*60)
    
    # 等待服务器启动
    print("\nWaiting for server to be ready...")
    time.sleep(2)
    
    results = []
    
    # 运行测试
    results.append(("Unary RPC", test_unary_rpc()))
    results.append(("Streaming RPC", test_streaming_rpc()))
    results.append(("Context Manager", test_context_manager()))
    results.append(("Connection Failure", test_connection_failure()))
    
    # 打印测试结果
    print("\n" + "="*60)
    print("Test Summary")
    print("="*60)
    
    passed = sum(1 for _, result in results if result)
    total = len(results)
    
    for name, result in results:
        status = "Success PASSED" if result else "False FAILED"
        print(f"{name:30s} {status}")
    
    print("-"*60)
    print(f"Total: {passed}/{total} tests passed")
    
    if passed == total:
        print("\nSuccess All tests passed!")
        return 0
    else:
        print(f"\nFalse {total - passed} test(s) failed")
        return 1


if __name__ == "__main__":
    sys.exit(main())
