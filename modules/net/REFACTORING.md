# Net 模块重构完成

## 📁 目录结构

```
modules/net/
├── CMakeLists.txt              # 模块级 CMake 配置
├── include/
│   └── net/                    # 公共头文件
│       ├── asio_io_context_pool.h
│       ├── databuffer.h
│       ├── httpclient.h
│       ├── httpclientpool.h
│       ├── httprouter.h
│       ├── httpserver.h
│       ├── httpsession.h
│       ├── session.h
│       ├── tcpserver.h
│       ├── tcpsession.h
│       ├── websocket.h
│       ├── websocket_router.h
│       ├── websocket_server.h
│       └── websocket_session.h
├── src/                        # 源文件
│   ├── asio_io_context_pool.cpp
│   ├── httpclient.cpp
│   ├── httpclientpool.cpp
│   ├── httprouter.cpp
│   ├── httpserver.cpp
│   ├── httpsession.cpp
│   ├── tcpserver.cpp
│   ├── tcpsession.cpp
│   ├── websocket_router.cpp
│   ├── websocket_server.cpp
│   └── websocket_session.cpp
├── lib/                        # 编译输出的库文件
│   └── net_lib.lib            # (编译后生成)
└── test/                       # 测试文件
    ├── CMakeLists.txt          # 测试 CMake 配置
    ├── bin/                    # 测试可执行文件输出目录
    ├── boostjson.cpp
    ├── httpclient.cpp
    ├── httpclientpool.cpp
    ├── httpserver.cpp
    ├── tcpserver.cpp
    └── websocket.cpp
```

## 🔧 修改的文件

### 1. modules/net/CMakeLists.txt
- 创建独立的静态库 `net_lib`
- 自动查找并链接 Boost (asio, system, json) 和 spdlog
- 支持可选的测试构建

### 2. modules/net/test/CMakeLists.txt
- 自动扫描所有测试文件
- 为每个测试创建独立的可执行文件
- 链接到 `net_lib`

### 3. 根目录 CMakeLists.txt
- 注释掉旧的 gRPC 子目录：`# add_subdirectory(grpc)`
- 添加 `add_subdirectory(modules/net)`
- 在主程序中链接 `net_lib`
- 注释掉主程序中的 `grpc_lib` 链接

### 4. 主程序链接更新
```cmake
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        log_lib
        net_lib  # ← 新增
        ...
        # grpc_lib  # ← 已注释
)
```

## ✅ 优势

### 编译速度提升
- **增量编译**：只修改 net 模块时，其他模块不需要重新编译
- **并行编译**：net_lib 可以与其他模块并行编译
- **缓存友好**：未修改的 .obj 文件可以复用

### 依赖管理清晰
```cmake
# 其他模块只需要链接 net_lib
target_link_libraries(other_module PRIVATE net_lib)
```

### 可复用性
- 其他项目可以直接使用 net_lib
- 可以轻松发布为独立的库

## 🚀 使用方法

### 编译
```powershell
cd build
cmake ..
cmake --build . --config Debug
```

编译后生成的文件：
- **库文件**: `modules/net/lib/net_lib.lib`
- **测试可执行文件**: `modules/net/test/bin/test_net_*.exe`

### 运行测试
```powershell
# 直接运行测试可执行文件
.\modules\net\test\bin\test_net_httpserver.exe
.\modules\net\test\bin\test_net_tcpserver.exe
.\modules\net\test\bin\test_net_websocket.exe
```

### 在其他模块中使用
```cpp
// 包含头文件
#include "net/httpserver.h"
#include "net/tcpserver.h"
#include "net/websocket.h"

// CMakeLists.txt 中链接
target_link_libraries(your_module PRIVATE net_lib)
```

## 📝 下一步

可以继续迁移其他模块：
1. ✅ log (已完成)
2. ✅ net (已完成)
3. ⏳ puller
4. ⏳ decoder
5. ⏳ preprocess
6. ⏳ postprocess
7. ⏳ alg (包含 gRPC)
8. ⏳ videopipeline

## 💡 注意事项

1. **头文件路径保持不变**：仍然是 `#include "net/httpserver.h"`
2. **向后兼容**：旧的 include/net/ 和 src/net/ 目录暂时保留作为备份
3. **测试隔离**：net 模块的测试完全独立，不依赖其他模块
4. **gRPC 已禁用**：主 CMake 中已注释掉 `add_subdirectory(grpc)`，等待迁移到 modules/alg/grpc/
