# OpenVINO 测试 - VS 调试快速参考

## 🎯 一键启动

### 在 Visual Studio 中：

1. **选择配置**：顶部下拉菜单 → `test_video_pipeline_openvino - Debug`
2. **按 F5**：开始调试

✅ 完成！环境变量和工作目录已自动配置。

---

## 📋 配置详情

### 工作目录
```
D:\file_mx\aaaaa\learncpp\modules\videopipeline\test
```

### 环境变量
```
OPENVINO_PLUGIN_PATHS=D:\file_mx\aaaaa\learncpp\modules\videopipeline\test\bin
PATH=D:\file_mx\aaaaa\learncpp\modules\videopipeline\test\bin;%PATH%
```

### 命令行参数
```
"http://127.0.0.1:8888/live/proxy_cam1.live.flv" "yolov5s.xml" "CPU" "60"
```

---

## 🔍 前置检查

运行前确认（PowerShell）：

```powershell
cd D:\file_mx\aaaaa\learncpp\modules\videopipeline\test
.\diagnostic_openvino.ps1
```

应该看到：
```
✓ All checks passed! You can run the test.
```

---

## ⚠️ 常见错误

### 错误 1: Available frontends: (empty)
**原因**：环境变量未设置  
**解决**：确保选择了正确的启动配置

### 错误 2: Unable to read the model
**原因**：模型文件不在工作目录  
**解决**：确认 `bin\yolov5s.xml` 和 `bin\yolov5s.bin` 存在

### 错误 3: DLL 加载失败
**原因**：PATH 未包含 bin 目录  
**解决**：检查环境变量配置

---

## 🛠️ 手动配置（备选）

如果自动配置不工作：

1. 右键项目 → **属性**
2. **配置属性 → 调试**
3. 设置：
   - 工作目录：`$(ProjectDir)`
   - 命令参数：见上方
   - 环境：见上方

---

## 📚 详细文档

- [完整 VS 配置指南](VS_DEBUG_CONFIG.md)
- [故障排查](TROUBLESHOOTING_OPENVINO.md)
- [解决方案](SOLUTION_OPENVINO_LOADING.md)

---

**提示**：遇到问题先运行诊断脚本！