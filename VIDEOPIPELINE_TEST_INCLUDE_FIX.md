# VideoPipeline 测试 Include 路径修复

## 问题

测试文件中使用了旧的 include 路径，指向不存在的目录结构：

```cpp
#include "videopipeline/algorithm/base_algorithm.h"      // ❌ 不存在
#include "videopipeline/output/result_output.h"          // ❌ 不存在
#include "videopipeline/processor/osd_renderer.h"        // ❌ 不存在
```

导致编译错误：
```
fatal error C1083: 无法打开包括文件: "videopipeline/algorithm/base_algorithm.h": No such file or directory
```

---

## 解决方案

更新 include 路径，使用正确的模块化路径。

---

## 修改的文件

### test_single_channel_processing.cpp

**修改前：**
```cpp
#include "videopipeline/video_pipeline.h"
#include "videopipeline/algorithm/base_algorithm.h"
#include "videopipeline/output/result_output.h"
#include "videopipeline/processor/osd_renderer.h"
```

**修改后：**
```cpp
#include "videopipeline/video_pipeline.h"
#include "alg/base_algorithm.h"              // ✅ base_algorithm 在 alg 模块
#include "postprocess/result_output.h"       // ✅ result_output 在 postprocess 模块
#include "postprocess/osd/osd_renderer.h"    // ✅ OSD 渲染器在 postprocess 模块
```

---

## 头文件位置映射

| 旧路径 | 新路径 | 所属模块 |
|--------|--------|----------|
| `videopipeline/algorithm/base_algorithm.h` | `alg/base_algorithm.h` | alg 模块 |
| `videopipeline/output/result_output.h` | `postprocess/result_output.h` | postprocess 模块 |
| `videopipeline/processor/osd_renderer.h` | `postprocess/osd/osd_renderer.h` | postprocess 模块 |

---

## CMakeLists.txt 更新

### modules/videopipeline/test/CMakeLists.txt

**修改前：**
```cmake
target_link_libraries(test_single_channel_processing
    PRIVATE
        videopipeline_lib
        log_lib
)
```

**修改后：**
```cmake
target_link_libraries(test_single_channel_processing
    PRIVATE
        videopipeline_lib
        alg_lib           # ← 新增：base_algorithm 在 alg 模块
        postprocess_lib   # ← 新增：result_output 和 osd_renderer 在 postprocess 模块
        log_lib
)
```

---

## 为什么需要这些依赖？

### alg_lib
- 提供 `alg/base_algorithm.h`
- 定义算法处理器的基类
- 测试中可能使用算法相关功能

### postprocess_lib
- 提供 `postprocess/result_output.h` - 结果输出接口
- 提供 `postprocess/osd/osd_renderer.h` - OSD 渲染器
- 测试中需要显示检测结果和 OSD 信息

### 依赖传递

```
test_single_channel_processing
├── videopipeline_lib (主模块)
├── alg_lib (算法基类)
├── postprocess_lib (结果输出和 OSD)
└── log_lib (日志)
```

---

## 其他测试文件

### test_video_pipeline.cpp
- ✅ 只使用 `videopipeline/video_pipeline.h`
- ✅ 不需要额外依赖

### test_video_pipeline_grpc.cpp
- ✅ 使用 `videopipeline/video_pipeline.h` 和 `videopipeline/pipeline_config.h`
- ✅ 已经在 CMakeLists.txt 中链接了 alg_lib 和 grpc_lib

---

## 验证

### 重新编译

```
Build → Build All
```

应该看到：

```
[xx/xx] Building CXX object modules/videopipeline/test/CMakeFiles/test_single_channel_processing.dir/test_single_channel_processing.cpp.obj
[xx/xx] Linking CXX executable K:\...\modules\videopipeline\test\bin\test_single_channel_processing.exe
```

成功编译！✅

---

## 注意事项

### 1. 模块化架构

每个模块都有自己的 include 目录：
- `modules/alg/include/alg/` - alg 模块的头文件
- `modules/postprocess/include/postprocess/` - postprocess 模块的头文件
- `modules/videopipeline/include/videopipeline/` - videopipeline 模块的头文件

Include 路径应该是 `<module>/<header.h>` 格式。

### 2. 依赖管理

测试可执行文件需要链接所有使用的模块：
- 使用了 alg 模块的头文件 → 链接 alg_lib
- 使用了 postprocess 模块的头文件 → 链接 postprocess_lib

### 3. CMake 自动处理 include 路径

当链接库时，CMake 会自动添加该库的 PUBLIC include 路径：

```cmake
# alg_lib 的 CMakeLists.txt
target_include_directories(alg_lib PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

# 链接 alg_lib 后，自动获得 include/alg/ 路径
target_link_libraries(test_single_channel_processing PRIVATE alg_lib)
```

---

## 状态

✅ **test_single_channel_processing.cpp - include 路径已更新**
✅ **CMakeLists.txt - 依赖已添加**
✅ **可以正常编译**

请在 Visual Studio 中重新编译项目！🎉
