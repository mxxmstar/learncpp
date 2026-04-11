# Service 模块重构完成

## 📁 目录结构

```
modules/service/
├── CMakeLists.txt              # 模块级 CMake 配置
├── include/
│   └── service/                # 公共头文件
│       ├── httpclient_pool_service.h
│       ├── http_server_service.h
│       ├── iservice.h
│       ├── service_manager.h
│       └── zlm_service.h
├── src/                        # 源文件
│   ├── httpclient_pool_service.cpp
│   ├── http_server_service.cpp
│   └── zlm_service.cpp
├── lib/                        # 编译输出的库文件
│   └── service_lib.lib        # (编译后生成)
└── test/                       # 测试文件
    ├── CMakeLists.txt          # 测试 CMake 配置
    ├── bin/                    # 测试可执行文件输出目录
    ├── test_grpc_hello.cpp
    └── test_service_arch.cpp
```

## 🔧 修改的文件

### 1. modules/service/CMakeLists.txt
- 创建独立的静态库 `service_lib`
- 依赖：yaml-cpp, log_lib, net_lib, zlmediakit_lib, config_lib, api_lib
- 测试开关默认为 `OFF`
- 支持 UTF-8 编码

### 2. modules/service/test/CMakeLists.txt
- 自动扫描所有测试文件
- 为每个测试创建独立的可执行文件
- 链接到 `service_lib`

### 3. 根目录 CMakeLists.txt
- 添加 `add_subdirectory(modules/service)`
- 在主程序中链接 `service_lib`

## ✅ 优势

### 1. 清晰的依赖管理
```cmake
# Service 模块依赖所有已迁移的模块
target_link_libraries(service_lib
    PUBLIC
        yaml-cpp::yaml-cpp
        log_lib
        net_lib
        zlmediakit_lib
        config_lib
        api_lib
)
```

### 2. 服务层抽象
- **IService 接口** - 统一的服务接口
- **ServiceManager** - 服务管理器
- **具体服务实现** - HTTP客户端池、HTTP服务器、ZLM服务

### 3. 编译速度提升
- **增量编译**：只修改 service 模块时，其他模块不需要重新编译
- **并行编译**：service_lib 可以与其他模块并行编译
- **缓存友好**：未修改的 .obj 文件可以复用

## 🚀 使用方法

### 编译
```powershell
cd build
cmake ..
cmake --build . --config Debug
```

编译后生成的文件：
- **库文件**: `modules/service/lib/service_lib.lib`
- **测试可执行文件** (如果启用): `modules/service/test/bin/test_service_*.exe`

### 启用测试
```powershell
# 默认测试是禁用的，需要显式启用
cmake .. -DBUILD_SERVICE_TESTS=ON
cmake --build . --config Debug

# 运行测试
.\modules\service\test\bin\test_service_arch.exe
```

### 在其他模块中使用
```cpp
// 包含头文件
#include "service/iservice.h"
#include "service/service_manager.h"
#include "service/zlm_service.h"

// CMakeLists.txt 中链接
target_link_libraries(your_module PRIVATE service_lib)
```

## 📊 当前已迁移的模块（7个）

```
modules/
├── log/          ✅ (BUILD_LOG_TESTS=OFF)
│   └── lib/log_lib.lib
├── net/          ✅ (BUILD_NET_TESTS=OFF)
│   └── lib/net_lib.lib
├── sqlite/       ✅ (BUILD_SQLITE_TESTS=ON)
│   └── lib/sqlite_lib.lib
├── zlmediakit/   ✅ (BUILD_ZLM_TESTS=ON)
│   └── lib/zlmediakit_lib.lib
├── config/       ✅ (BUILD_CONFIG_TESTS=OFF)
│   └── lib/config_lib.lib
├── api/          ✅ (BUILD_API_TESTS=OFF)
│   └── lib/api_lib.lib
└── service/      ✅ (BUILD_SERVICE_TESTS=OFF)
    └── lib/service_lib.lib
```

## 🔍 依赖关系图

```
基础层:
  log_lib

第二层:
  net_lib → log_lib
  sqlite_lib → log_lib
  config_lib → log_lib

第三层:
  zlmediakit_lib → log_lib + net_lib

第四层:
  api_lib → log_lib + net_lib + zlmediakit_lib + config_lib

第五层 (最高层):
  service_lib → log_lib + net_lib + zlmediakit_lib + config_lib + api_lib
```

## 💡 注意事项

1. **头文件路径保持不变**：`#include "service/iservice.h"`

2. **向后兼容**：旧的 include/service/ 和 src/service/ 目录暂时保留作为备份

3. **测试默认禁用**：`BUILD_SERVICE_TESTS` 默认为 `OFF`，需要时手动启用

4. **依赖最多**：service 模块依赖所有已迁移的模块，是最上层的业务逻辑

5. **依赖顺序重要**：在主 CMake 中，service 必须最后添加

## 📝 下一步

可以继续迁移剩余模块：
1. ✅ log (已完成)
2. ✅ net (已完成)
3. ✅ sqlite (已完成)
4. ✅ zlmediakit (已完成)
5. ✅ config (已完成)
6. ✅ api (已完成)
7. ✅ service (已完成)
8. ⏳ puller
9. ⏳ decoder
10. ⏳ preprocess
11. ⏳ postprocess
12. ⏳ alg (包含 gRPC)
13. ⏳ videopipeline

## 🎯 模块化进展

目前已迁移 **7个模块**，占总数约 **54%** (7/13)

**剩余模块：**
- puller - 拉流模块
- decoder - 解码模块
- preprocess - 预处理模块
- postprocess - 后处理模块
- alg - 算法模块（包含 gRPC）
- videopipeline - 流水线核心
