# Visual Studio 调试 OpenVINO 测试配置指南

## 🎯 问题说明

在 Visual Studio 中直接调试 `test_video_pipeline_openvino` 时，会遇到以下错误：

```
Unable to read the model: yolov5s.xml
Available frontends: (empty)
```

**原因**：VS 调试时不会自动设置 `OPENVINO_PLUGIN_PATHS` 环境变量。

## ✅ 解决方案

### 方案 1：使用已配置的启动配置（推荐）✨

我已经为您创建了专门的调试配置。

**步骤**：

1. **重新加载项目**（如果 VS 已打开）
2. **选择调试配置**：
   - 在 VS 顶部的下拉菜单中选择：
     ```
     test_video_pipeline_openvino - Debug
     ```
3. **按 F5 开始调试**

配置会自动设置：
- ✓ 工作目录：`modules/videopipeline/test`
- ✓ 环境变量：`OPENVINO_PLUGIN_PATHS`
- ✓ 命令行参数：流地址、模型路径等

### 方案 2：手动配置项目属性

如果方案 1 不工作，可以手动配置：

#### 步骤 1：打开项目属性

1. 在 **解决方案资源管理器** 中找到 `test_video_pipeline_openvino`
2. 右键点击 → **属性 (Properties)**

#### 步骤 2：设置调试配置

导航到：**配置属性 → 调试**

**工作目录 (Working Directory)**：
```
$(ProjectDir)
```
或
```
D:\file_mx\aaaaa\learncpp\modules\videopipeline\test
```

**命令参数 (Command Arguments)**：
```
"http://127.0.0.1:8888/live/proxy_cam1.live.flv" "yolov5s.xml" "CPU" "60"
```

**环境 (Environment)**：
```
OPENVINO_PLUGIN_PATHS=$(ProjectDir)bin
PATH=$(ProjectDir)bin;%PATH%
```

点击 **应用** → **确定**

#### 步骤 3：开始调试

按 **F5** 或点击 **本地 Windows 调试器**

### 方案 3：使用 CMake Settings（CMake 模式）

如果您使用的是 CMake 模式，编辑 `.vs/launch.vs.json`（已完成）。

## 🔍 验证配置

### 检查点 1：模型文件位置

确保 `yolov5s.xml` 和 `yolov5s.bin` 在正确位置：

```powershell
cd D:\file_mx\aaaaa\learncpp\modules\videopipeline\test
dir bin\yolov5s.*
```

应该看到：
```
yolov5s.xml  (287 KB)
yolov5s.bin  (14 MB)
```

### 检查点 2：DLL 文件完整性

```powershell
cd D:\file_mx\aaaaa\learncpp\modules\videopipeline\test\bin
dir *openvino*.dll | Measure-Object
```

应该显示 **15 个 DLL 文件**。

关键 DLL：
- ✓ `openvino.dll`
- ✓ `openvino_ir_frontend.dll` ← **必须存在**
- ✓ `openvino_intel_cpu_plugin.dll` ← **必须存在**

### 检查点 3：运行诊断脚本

```powershell
cd D:\file_mx\aaaaa\learncpp\modules\videopipeline\test
.\diagnostic_openvino.ps1
```

应该看到：
```
✓ All checks passed! You can run the test.
```

## 🐛 常见问题

### Q1: 仍然提示 "Available frontends: (empty)"

**原因**：环境变量未生效

**解决**：
1. 确认在 VS 中选择了正确的启动配置
2. 重启 Visual Studio
3. 检查环境变量是否正确设置：
   - 在代码中添加临时调试输出：
   ```cpp
   const char* plugin_path = std::getenv("OPENVINO_PLUGIN_PATHS");
   std::cout << "OPENVINO_PLUGIN_PATHS: " << (plugin_path ? plugin_path : "NOT SET") << std::endl;
   ```

### Q2: 找不到模型文件

**原因**：工作目录不正确

**解决**：
1. 检查工作目录设置是否为 `$(ProjectDir)`
2. 或者使用绝对路径：
   ```
   "D:\file_mx\aaaaa\learncpp\modules\videopipeline\test\bin\yolov5s.xml"
   ```

### Q3: DLL 加载失败

**原因**：PATH 环境变量未包含 bin 目录

**解决**：
确保环境中设置了：
```
PATH=$(ProjectDir)bin;%PATH%
```

## 📝 快速检查清单

在开始调试前，确认：

- [ ] 模型文件在 `bin` 目录：`yolov5s.xml` + `yolov5s.bin`
- [ ] OpenVINO DLL 在 `bin` 目录（15 个文件）
- [ ] 选择了正确的启动配置：`test_video_pipeline_openvino - Debug`
- [ ] 工作目录设置为项目目录
- [ ] 环境变量 `OPENVINO_PLUGIN_PATHS` 已设置
- [ ] 运行过诊断脚本并显示通过

## 💡 调试技巧

### 添加环境变量检查代码

在 `main()` 函数开头添加：

```cpp
#include <cstdlib>
#include <iostream>

int main(int argc, char* argv[]) {
    // 检查环境变量
    const char* plugin_path = std::getenv("OPENVINO_PLUGIN_PATHS");
    std::cout << "=== Environment Check ===" << std::endl;
    std::cout << "OPENVINO_PLUGIN_PATHS: " 
              << (plugin_path ? plugin_path : "NOT SET") << std::endl;
    
    const char* path = std::getenv("PATH");
    std::cout << "PATH contains bin: " 
              << (path && std::string(path).find("bin") != std::string::npos 
                  ? "YES" : "NO") << std::endl;
    std::cout << "=========================" << std::endl;
    
    // ... 原有代码
}
```

### 使用断点验证

在 `OpenVINOBackend::initialize()` 处设置断点，检查：
1. `config.openvino.model_path` 的值
2. 文件是否存在
3. 环境变量是否设置

## 🚀 成功标志

调试成功后，应该看到：

```
Creating inference engine: openvino_cpu
Loading OpenVINO model from: yolov5s.xml
[OpenVINOBackend] Initialized: model=yolov5s.xml, device=CPU, batch=1
✓ VideoPipeline started successfully

Waiting for frames...
(Press Ctrl+C to stop early)

--- Statistics at 5s ---
  Received:  150 frames
  Decoded:   148 frames
  Processed: 145 frames
```

## 📚 相关文档

- [故障排查指南](TROUBLESHOOTING_OPENVINO.md)
- [解决方案总结](SOLUTION_OPENVINO_LOADING.md)
- [快速开始](QUICKSTART_OPENVINO_TEST.md)

---

**最后更新**: 2026-05-09  
**状态**: ✅ VS 调试配置已完成