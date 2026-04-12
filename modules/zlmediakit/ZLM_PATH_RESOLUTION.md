# ZLMediaKit 路径自动调整方案

## 问题描述

之前的代码使用 `std::filesystem::current_path().parent_path()` 来定位 ZLMediaKit 可执行文件，但这在不同场景下行为不一致：

### 问题场景

1. **主程序运行**：
   - 可执行文件：`bin/Debug/MySelfContainedApp.exe`
   - current_path() = `bin/Debug`
   - parent_path() = 项目根目录 ✓ 正确

2. **测试程序运行**：
   - 可执行文件：`modules/zlmediakit/test/bin/Debug/test_zlm_zlm.exe`
   - current_path() = `modules/zlmediakit/test/bin/Debug`
   - parent_path() = `modules/zlmediakit/test/bin` ✗ 错误！

3. **从其他目录运行**：
   - 如果用户从任意目录运行程序，parent_path() 完全不相关 ✗ 错误！

## 解决方案

### 1. CMake 编译时定义项目根目录

在 `modules/zlmediakit/CMakeLists.txt` 中添加：

```cmake
# 定义项目根目录（用于定位 tools 目录）
target_compile_definitions(zlmediakit_lib PRIVATE
    PROJECT_ROOT_DIR="${CMAKE_SOURCE_DIR}"
)
```

这样在编译时会将项目根目录作为宏定义嵌入到代码中。

### 2. 代码中使用 PROJECT_ROOT_DIR

更新 `zlm_manager.cpp` 中的两个函数：

#### createProcessConfig 函数
```cpp
static ZLMProcessManager::Config createProcessConfig(const ZlmConfig& zlm_config) {
    ZLMProcessManager::Config cfg;
    cfg.debug_terminal = zlm_config.debug_terminal;
    
    // 使用编译时定义的项目根目录，确保在任何位置运行都能找到 tools 目录
#ifdef PROJECT_ROOT_DIR
    std::filesystem::path project_root(PROJECT_ROOT_DIR);
#else
    // 如果没有定义 PROJECT_ROOT_DIR，回退到当前路径的父目录
    std::filesystem::path project_root = std::filesystem::current_path().parent_path();
    LOG_MAIN_WARN_AT("ZLMProcessManager: PROJECT_ROOT_DIR not defined, using parent of current path");
#endif
    
#ifdef _WIN32    
    auto work_dir_path = project_root / "tools" / "win32" / "zlmediakit";
    cfg.work_dir = work_dir_path.string();
#else
    auto work_dir_path = project_root / "tools" / "linux" / "zlmediakit";
    cfg.work_dir = work_dir_path.string();
#endif
    return cfg;
}
```

#### GetZlmediakitPath 函数
```cpp
std::string ZLMProcessManager::GetZlmediakitPath() {
    std::filesystem::path exec_path;
    
    // 使用编译时定义的项目根目录，确保在任何位置运行都能找到 tools 目录
#ifdef PROJECT_ROOT_DIR
    std::filesystem::path project_root(PROJECT_ROOT_DIR);
#else
    // 如果没有定义 PROJECT_ROOT_DIR，回退到当前路径的父目录
    std::filesystem::path project_root = std::filesystem::current_path().parent_path();
    LOG_MAIN_WARN_AT("ZLMProcessManager: PROJECT_ROOT_DIR not defined, using parent of current path");
#endif
    
#ifdef _WIN32
    exec_path = project_root / "tools" / "win32" / "zlmediakit" / "MediaServer.exe";
#else
    exec_path = project_root / "tools" / "linux" / "zlmediakit" / "MediaServer";
#endif    
    
    LOG_MAIN_INFO_AT("ZLMProcessManager: Looking for ZLMediaKit at: {}", exec_path.string());
    LOG_MAIN_INFO_AT("ZLMProcessManager: Project root: {}", project_root.string());
    LOG_MAIN_INFO_AT("ZLMProcessManager: Current path: {}", std::filesystem::current_path().string());
    
    if (std::filesystem::exists(exec_path)) {
        LOG_MAIN_INFO_AT("ZLMProcessManager: Found ZLMediaKit at: {}", exec_path.string());
        return exec_path.string();
    }
    LOG_MAIN_ERROR_AT("ZLMProcessManager: ZLMediaKit not found at: {}", exec_path.string());
    return "";
}
```

## 优势

### 1. 一致性
无论从哪里运行程序，都能正确找到 ZLMediaKit：
- ✅ 主程序：`bin/Debug/MySelfContainedApp.exe`
- ✅ 测试程序：`modules/zlmediakit/test/bin/Debug/test_zlm_zlm.exe`
- ✅ 任意目录：从任何位置运行都可正常工作

### 2. 向后兼容
如果 `PROJECT_ROOT_DIR` 未定义（例如手动编译时），代码会回退到原来的行为，并输出警告日志。

### 3. 易于调试
添加了详细的日志输出，显示：
- 项目根目录
- 当前工作目录
- 正在查找的路径
- 是否找到文件

## 使用示例

### 编译后运行

```powershell
# 从项目根目录运行
cd K:\code_mx\llfc_chat_go\learncpp
.\bin\Debug\MySelfContainedApp.exe

# 从任意目录运行
cd C:\Users\mx
K:\code_mx\llfc_chat_go\learncpp\bin\Debug\MySelfContainedApp.exe

# 运行测试程序
.\modules\zlmediakit\test\bin\Debug\test_zlm_zlm.exe
```

所有情况都会正确找到：
```
K:\code_mx\llfc_chat_go\learncpp\tools\win32\zlmediakit\MediaServer.exe
```

## 日志输出示例

```
[INFO] ZLMProcessManager: Project root: K:\code_mx\llfc_chat_go\learncpp
[INFO] ZLMProcessManager: Current path: K:\code_mx\llfc_chat_go\learncpp\bin\Debug
[INFO] ZLMProcessManager: Looking for ZLMediaKit at: K:\code_mx\llfc_chat_go\learncpp\tools\win32\zlmediakit\MediaServer.exe
[INFO] ZLMProcessManager: Found ZLMediaKit at: K:\code_mx\llfc_chat_go\learncpp\tools\win32\zlmediakit\MediaServer.exe
```

## 注意事项

1. **重新编译**：修改 CMakeLists.txt 后需要重新运行 CMake 配置和编译
2. **路径分隔符**：Windows 使用反斜杠 `\`，Linux 使用正斜杠 `/`，`std::filesystem::path` 会自动处理
3. **相对路径 vs 绝对路径**：使用 `PROJECT_ROOT_DIR` 构建的是绝对路径，更加可靠

## 扩展到其他模块

如果其他模块也需要类似的功能（如 ffmpeg、其他外部工具），可以采用相同的方案：

```cmake
# 在其他模块的 CMakeLists.txt 中添加
target_compile_definitions(your_module PRIVATE
    PROJECT_ROOT_DIR="${CMAKE_SOURCE_DIR}"
)
```

---

**更新日期**: 2026-04-12  
**状态**: ✅ 已完成
