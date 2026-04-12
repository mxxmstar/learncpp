# GRPC 模块 Include 路径修复

## 问题

编译 grpc 模块时出现错误：

```
fatal error C1083: 无法打开包括文件: "grpc_client.h": No such file or directory
fatal error C1083: 无法打开包括文件: "grpc_server.h": No such file or directory
fatal error C1083: 无法打开包括文件: "hello_grpc_service.h": No such file or directory
fatal error C1083: 无法打开包括文件: "video_grpc_client.h": No such file or directory
```

## 原因

grpc 模块的源文件中使用了错误的 include 路径，缺少模块前缀 `grpc/`。

## 解决方案

修复所有 grpc 源文件的 include 路径：

### 修改的文件

| 文件 | 修改前 | 修改后 |
|------|--------|--------|
| `modules/grpc/src/grpc_client.cpp` | `#include "grpc_client.h"` | `#include "grpc/grpc_client.h"` |
| `modules/grpc/src/grpc_server.cpp` | `#include "grpc_server.h"` | `#include "grpc/grpc_server.h"` |
| `modules/grpc/src/hello_grpc_service.cpp` | `#include "hello_grpc_service.h"` | `#include "grpc/hello_grpc_service.h"` |
| `modules/grpc/src/video_grpc_client.cpp` | `#include "video_grpc_client.h"` | `#include "grpc/video_grpc_client.h"` |

## Include 路径规范

所有模块都必须使用 `<module_name>/<header.h>` 的格式：

```cpp
// ✅ 正确
#include "grpc/grpc_client.h"
#include "grpc/video_grpc_client.h"
#include "alg/grpc/grpc_to_alg.h"

// ❌ 错误（缺少模块前缀）
#include "grpc_client.h"
#include "video_grpc_client.h"
```

## 验证

重新编译：

```
Build → Build All
```

应该看到：

```
[xx/xx] Building CXX object modules/grpc/CMakeFiles/grpc_lib.dir/src/grpc_client.cpp.obj
[xx/xx] Building CXX object modules/grpc/CMakeFiles/grpc_lib.dir/src/grpc_server.cpp.obj
[xx/xx] Building CXX object modules/grpc/CMakeFiles/grpc_lib.dir/src/hello_grpc_service.cpp.obj
[xx/xx] Building CXX object modules/grpc/CMakeFiles/grpc_lib.dir/src/video_grpc_client.cpp.obj
```

成功编译！✅

## 状态

✅ **所有 grpc 源文件的 include 路径已修正**
✅ **符合模块化 include 路径规范**
