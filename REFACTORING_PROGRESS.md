# 架构重构进度报告

## ✅ 已完成的工作

### 1. Net 模块拆分 ✅
- http_client/
- http_server/
- tcp_server/
- websocket/
- io_context_pool/

### 2. Common 模块扩展 ✅
- ✅ 复制 log 模块到 common/include/common/log/
- ✅ 复制 config 模块到 common/include/common/config/
- ✅ 复制源文件到 common/src/
- ✅ 更新 CMakeLists.txt（添加 spdlog, fmt, yaml-cpp, Boost 依赖）

### 3. Service 模块创建 ✅
- ✅ 创建 service/http_server/ (HttpServerService)
- ✅ 创建 service/http_client/ (HttpClientPoolService)
- ✅ 创建 service/zlm/ (ZLMService)
- ✅ 创建各子模块的 CMakeLists.txt
- ✅ 创建主 service/CMakeLists.txt

### 4. CMake 配置更新 ✅
- ✅ 主 CMakeLists.txt 添加 service 模块
- ✅ web/CMakeLists.txt 移除 service 源文件，添加 service 库依赖
- ✅ zlmediakit/CMakeLists.txt 移除 service 源文件
- ✅ common/CMakeLists.txt 添加 log 和 config 依赖

### 5. 头文件路径更新 ✅
- ✅ app_with_framework.cpp: 更新 Service include 路径
- ✅ Service 文件内部: 更新 include 路径
- ⚠️ 其他文件的 log/config 引用尚未批量更新

---

## ⚠️ 待完成工作

### 高优先级（必须完成才能编译）

#### 1. 批量更新所有 log 和 config 的 include 路径
需要替换的文件类型：
```cpp
// 之前
#include "log/logmanager.h"
#include "config/common_config.h"

// 之后
#include "common/log/logmanager.h"
#include "common/config/common_config.h"
```

**影响范围**：
- modules/net/*/src/*.cpp
- modules/web/src/api/*.cpp
- modules/zlmediakit/src/*.cpp
- modules/camera/src/*.cpp
- modules/application/src/*.cpp
- test/**/*.cpp
- apps/*.cpp

**估计文件数**：约 50-80 个文件

#### 2. 删除旧文件（编译通过后）
- modules/log/ （已移动到 common）
- modules/config/ （已移动到 common）
- modules/web/include/web/service/ （已移动到 service）
- modules/web/src/service/ （已移动到 service）
- modules/zlmediakit/include/zlmediakit/service/ （已移动到 service）
- modules/zlmediakit/src/service/ （已移动到 service）

---

### 中优先级（优化项）

#### 3. 更新测试文件
- modules/net/test/*.cpp
- modules/web/test/*.cpp
- modules/zlmediakit/test/*.cpp
- test/**/*.cpp

#### 4. 清理空的 include/net 和 src 目录
modules/net/include/net/ 和 modules/net/src/ 现在应该为空

---

## 🔧 快速修复方案

### 方案 A：使用 PowerShell 脚本批量替换

创建并执行以下脚本：

```powershell
# update_all_includes.ps1
$rootPath = "d:\file_mx\aaaaa\learncpp"

$replacements = @{
    '#include "log/' = '#include "common/log/';
    '#include "config/' = '#include "common/config/';
}

$files = Get-ChildItem -Path $rootPath -Include *.h,*.cpp -Recurse -File | 
    Where-Object { $_.FullName -notmatch '\\out\\|\\.git\\|vcpkg\\' }

$updatedCount = 0
foreach ($file in $files) {
    $content = Get-Content $file.FullName -Raw -Encoding UTF8
    $originalContent = $content
    
    foreach ($key in $replacements.Keys) {
        if ($content -match [regex]::Escape($key)) {
            $content = $content -replace [regex]::Escape($key), $replacements[$key]
        }
    }
    
    if ($content -ne $originalContent) {
        Set-Content -Path $file.FullName -Value $content -Encoding UTF8 -NoNewline
        $updatedCount++
    }
}

Write-Host "Updated $updatedCount files"
```

### 方案 B：通过编译错误逐个修复

1. 尝试编译
2. 根据编译错误提示，逐个修复 include 路径
3. 重复直到编译通过

**优点**：更精确，不会误改
**缺点**：耗时长

---

## 📊 新架构概览

```
modules/
├── common/              ← 通用基础模块
│   ├── include/common/
│   │   ├── log/        ← 日志（从 modules/log 移动）
│   │   ├── config/     ← 配置（从 modules/config 移动）
│   │   └── service/    ← IService 接口
│   └── src/            ← log 和 config 的实现
│
├── service/             ← 新：所有 Service 实现
│   ├── http_server/    ← HttpServerService
│   ├── http_client/    ← HttpClientPoolService
│   └── zlm/            ← ZLMService
│
├── net/                 ← 网络底层库（已拆分）
│   ├── http_client/
│   ├── http_server/
│   ├── tcp_server/
│   ├── websocket/
│   └── io_context_pool/
│
├── zlmediakit/          ← ZLMediaKit 封装（不含 Service）
├── web/                 ← Web API 层（不含 Service）
├── camera/              ← Camera 业务逻辑
├── application/         ← Application 框架
└── ...
```

---

## 🎯 下一步行动

**建议立即执行**：

1. **批量更新 include 路径**（方案 A）
   ```bash
   cd d:\file_mx\aaaaa\learncpp
   powershell -ExecutionPolicy Bypass -File update_all_includes.ps1
   ```

2. **重新配置 CMake**
   ```bash
   cd out\build\x64-Debug
   cmake ..\..\..
   ```

3. **编译验证**
   ```bash
   cmake --build .
   ```

4. **根据编译错误修复问题**

5. **删除旧文件**

---

## 💡 注意事项

1. **备份代码**：确保已 git commit
2. **编译验证**：每步都要编译验证
3. **逐步删除**：确认编译通过后再删除旧文件
4. **测试功能**：重构后需要测试所有功能是否正常

---

## 📝 重构收益

1. **清晰的职责分离**：
   - common: 基础工具
   - service: 服务实现
   - net: 网络底层
   - web: API 层

2. **更好的可维护性**：每个模块职责单一

3. **更容易测试**：可以独立测试每个模块

4. **灵活的依赖管理**：避免循环依赖
