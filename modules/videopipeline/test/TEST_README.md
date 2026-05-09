# VideoPipeline OpenVINO 测试

## 🚀 快速开始

### 1. 编译
```bash
cd build
cmake ..
make test_video_pipeline_openvino
```

### 2. 运行

#### 方式 A：命令行（推荐用于快速测试）
```bash
# Windows
run_openvino_test.bat

# Linux/macOS
./run_openvino_test.sh
```

#### 方式 B：Visual Studio 调试（推荐用于开发）

**步骤**：
1. 在 VS 顶部选择启动配置：`test_video_pipeline_openvino - Debug`
2. 按 **F5** 开始调试

配置已自动设置：
- ✓ 工作目录
- ✓ 环境变量（OPENVINO_PLUGIN_PATHS）
- ✓ 命令行参数

详细配置说明见：[VS_DEBUG_CONFIG.md](VS_DEBUG_CONFIG.md)

### 3. 高级用法
```bash
./bin/test_video_pipeline_openvino \
    "http://127.0.0.1/live/proxy_cam1.live.flv" \
    "/path/to/model.xml" \
    "CPU" \
    60
```

## 📚 文档

- **快速开始**: [QUICKSTART_OPENVINO_TEST.md](QUICKSTART_OPENVINO_TEST.md)
- **详细文档**: [README_OPENVINO_TEST.md](README_OPENVINO_TEST.md)
- **完成总结**: [OPENVINO_TEST_SUMMARY.md](OPENVINO_TEST_SUMMARY.md)
- **验证清单**: [VERIFICATION_CHECKLIST.md](VERIFICATION_CHECKLIST.md)

## 🎯 测试内容

测试完整的 **puller → decoder → openvino** 流程：

```
┌──────────┐     ┌──────────┐     ┌──────────────┐
│  Puller  │────▶│ Decoder  │────▶│ OpenVINO     │
│ (FLV/RTSP)│    │(FFmpeg)  │    │ Backend      │
└──────────┘     └──────────┘     └──────────────┘
```

## ✨ 特性

- ✅ 完整流程测试
- ✅ 零拷贝架构验证
- ✅ 优雅关闭机制
- ✅ 详细统计信息
- ✅ 跨平台支持
- ✅ 灵活的配置

## 🔧 参数说明

| 参数 | 说明 | 默认值 |
|------|------|--------|
| stream_url | 视频流地址 | `http://127.0.0.1/live/proxy_cam1.live.flv` |
| model_path | OpenVINO 模型路径 | 空（使用 NullBackend） |
| device | 推理设备 | `CPU` |
| duration | 测试时长（秒） | `60` |

## 💡 提示

1. **首次运行**: 先不带模型测试，确保基础流程正常
2. **需要模型**: 从 OpenVINO Model Zoo 下载或使用自己的模型
3. **查看日志**: 详细日志输出到控制台和日志文件
4. **性能调优**: 参考详细文档中的性能优化建议

## ❓ 问题排查

遇到问题？
1. **先运行诊断脚本**：`.\diagnostic_openvino.ps1`
2. **查看详细文档**：[TROUBLESHOOTING_OPENVINO.md](TROUBLESHOOTING_OPENVINO.md)
3. **查看解决方案**：[SOLUTION_OPENVINO_LOADING.md](SOLUTION_OPENVINO_LOADING.md)

---

**Happy Testing! 🎉**