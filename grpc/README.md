# gRPC 独立库

这是一个独立的 gRPC 库模块，提供简单的 gRPC 服务端和客户端封装。

## 目录结构

```
grpc/
├── CMakeLists.txt          # 独立的 CMake 配置
├── proto/                  # Proto 定义文件
│   └── hello.proto
├── generated/              # 自动生成的 gRPC 代码
│   ├── hello.pb.h
│   ├── hello.pb.cc
│   ├── hello.grpc.pb.h
│   └── hello.grpc.pb.cc
├── include/                # 公共头文件
│   ├── grpc_server.h       # gRPC 服务端基类
│   ├── grpc_client.h       # gRPC 客户端基类
│   └── hello_grpc_service.h # Hello 服务
└── src/                    # 实现文件
    ├── grpc_server.cpp
    ├── grpc_client.cpp
    └── hello_grpc_service.cpp
```

## 编译

gRPC 库作为主项目的子目录自动编译：

```cmake
# 在主 CMakeLists.txt 中
add_subdirectory(grpc)
```

## 使用

### 在其他项目中链接

```cmake
# 链接 grpc_lib
target_link_libraries(your_target PRIVATE grpc_lib)

# 包含头文件
#include "hello_grpc_service.h"
```

### 示例：启动服务器

```cpp
#include "hello_grpc_service.h"

using namespace grpc_module;

int main() {
    HelloGrpcServer server("0.0.0.0:50051");
    server.Start();
    
    std::cout << "Server is running. Press Enter to stop..." << std::endl;
    std::cin.get();
    
    server.Stop();
    server.Wait();
    
    return 0;
}
```

### 示例：客户端调用

```cpp
#include "hello_grpc_service.h"

using namespace grpc_module;

int main() {
    HelloGrpcClient client("localhost:50051");
    
    if (!client.WaitForConnected(5)) {
        std::cerr << "Failed to connect to server" << std::endl;
        return 1;
    }
    
    simple_grpc::HelloResponse response;
    if (client.SayHello("World", &response)) {
        std::cout << "Response: " << response.message() << std::endl;
    }
    
    return 0;
}
```

## 添加新的 Proto 文件

1. 在 `proto/` 目录中添加 `.proto` 文件
2. 在 `CMakeLists.txt` 的 `add_custom_command` 中添加新的 proto 文件
3. 重新编译项目

## 架构优势

- ✅ **独立性**：gRPC 库完全独立，可以单独编译和测试
- ✅ **避免依赖循环**：生成的代码在库内部处理，不会与主项目产生循环依赖
- ✅ **易于维护**：所有 gRPC 相关代码集中在一个目录
- ✅ **可复用**：其他项目也可以链接这个库

## 依赖

- gRPC
- Protobuf
- Boost.Asio
- Boost.System
