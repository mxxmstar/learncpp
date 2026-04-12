# Web 模块（合并了 API 和 Service）

## 📁 目录结构

```
modules/web/
├── CMakeLists.txt              # 模块级 CMake 配置
├── include/
│   └── web/
│       ├── api/                # API 路由和处理
│       │   ├── api_router_registrar.h
│       │   ├── stream_api_handler.h
│       │   └── system_api_handler.h
│       └── service/            # 服务管理
│           ├── httpclient_pool_service.h
│           ├── http_server_service.h
│           ├── iservice.h
│           ├── service_container.h
│           └── zlm_service.h
├── src/
│   ├── api/                    # API 实现
│   │   ├── api_router_registrar.cpp
│   │   ├── stream_api_handler.cpp
│   │   └── system_api_handler.cpp
│   └── service/                # 服务实现
│       ├── httpclient_pool_service.cpp
│       ├── http_server_service.cpp
│       └── zlm_service.cpp
├── lib/                        # 编译输出的库文件
│   └── web_lib.lib            # (编译后生成)
└── test/                       # 测试文件
    ├── CMakeLists.txt
    ├── bin/
    └── test_service_arch.cpp
```

## 🔧 设计说明

### 为什么合并 API 和 Service？

1. **消除循环依赖**：api 模块和 service 模块相互依赖，合并后避免循环依赖问题
2. **逻辑内聚**：API 和 Service 都是处理 HTTP/Web 请求的相关功能，合并后更符合高内聚原则
3. **简化依赖管理**：减少模块数量，降低维护成本

### 模块职责

- **API 层**：负责 HTTP 路由注册、请求处理和响应生成
- **Service 层**：负责业务逻辑封装、服务生命周期管理

## ✅ 优势

### 1. 清晰的依赖管理
```cmake
# Web 模块的依赖（合并了 api 和 service）
target_link_libraries(web_lib
    PUBLIC
        nlohmann_json::nlohmann_json
        yaml-cpp::yaml-cpp
        log_lib
        net_lib
        zlmediakit_lib
        config_lib
)
```

### 2. 编译速度提升
- **增量编译**：只修改 web 模块时，其他模块不需要重新编译
- **并行编译**：web_lib 可以与其他模块并行编译
- **缓存友好**：未修改的 .obj 文件可以复用

## 🚀 使用方法

### 编译
```powershell
cd build
cmake ..
cmake --build . --config Debug
```

编译后生成的文件：
- **库文件**: `modules/web/lib/web_lib.lib`

### 启用测试
```powershell
# 启用 Web 测试
cmake .. -DBUILD_WEB_TESTS=ON
cmake --build . --config Debug

# 运行测试
.\modules\web\test\bin\test_web_service_arch.exe
```

### 在其他模块中使用
```cpp
// API 相关
#include "web/api/api_router_registrar.h"
#include "web/api/stream_api_handler.h"

// Service 相关
#include "web/service/iservice.h"
#include "web/service/service_container.h"
#include "web/service/zlm_service.h"

// CMakeLists.txt 中链接
target_link_libraries(your_module PRIVATE web_lib)
```

## 📊 依赖关系图

```
基础层:
  log_lib

第二层:
  net_lib → log_lib
  config_lib → log_lib

第三层:
  zlmediakit_lib → log_lib + net_lib

第四层 (最高层):
  web_lib → log_lib + net_lib + zlmediakit_lib + config_lib
```

## 💡 注意事项

1. **头文件路径变化**：
   - 旧：`#include "api/xxx.h"` → 新：`#include "web/api/xxx.h"`
   - 旧：`#include "service/xxx.h"` → 新：`#include "web/service/xxx.h"`

2. **向后兼容**：旧的 modules/api/ 和 modules/service/ 目录暂时保留作为备份

3. **测试默认禁用**：`BUILD_WEB_TESTS` 默认为 `OFF`，需要时手动启用

4. **依赖顺序重要**：在主 CMake 中，web 模块必须在 config 之后添加

## 📝 迁移历史

- **之前**：api 模块和 service 模块分开，存在循环依赖
- **现在**：合并为 web 模块，消除循环依赖，提高代码内聚性
