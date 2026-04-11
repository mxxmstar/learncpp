# 模块迁移测试指南

## 📋 已迁移的模块

### 1. Puller（拉流模块）
- **位置**: `include/puller/`, `src/puller/`
- **测试**: `test/puller/test_zlm_httpflv_puller.cpp`
- **启用**: `-DBUILD_PULLER_TESTS=ON`

### 2. Decoder（解码模块）
- **位置**: `include/decoder/`, `src/decoder/`
- **测试**: `test/decoder/test_ffmpeg_decoder.cpp`
- **启用**: `-DBUILD_DECODER_TESTS=ON`

### 3. Preprocess（预处理模块）
- **位置**: `include/preprocess/format_converter/`, `src/preprocess/format_converter/`
- **测试**: `test/preprocess/format_converter/test_format_converter.cpp`
- **启用**: `-DBUILD_PREPROCESS_TESTS=ON`

### 4. Postprocess（后处理模块）
- **位置**: `include/postprocess/osd/`, `src/postprocess/osd/`
- **测试**: `test/postprocess/osd/test_osd_renderer.cpp`
- **启用**: `-DBUILD_POSTPROCESS_TESTS=ON`

### 5. Alg（算法模块）
- **位置**: `include/alg/`, `src/alg/`
- **子模块**: `alg/grpc/` (GrpcToAlg, GrpcVideoSender)
- **测试**: `test/alg/grpc/test_grpc_to_alg.cpp`
- **启用**: `-DBUILD_ALG_TESTS=ON`

### 6. VideoPipeline（流水线核心）
- **位置**: `include/videopipeline/`, `src/videopipeline/`
- **包含**: control 层（pipeline_config, video_pipeline_manager）
- **测试**: `test/videopipeline/*.cpp`
- **启用**: `-DBUILD_VIDEO_PIPELINE_TESTS=ON`

## 🚀 快速开始

### 方法 1: 使用自动化脚本（推荐）

```powershell
.\build_and_test.ps1
```

### 方法 2: 手动编译

```powershell
# 创建 build 目录
mkdir build
cd build

# 配置 CMake（启用所有新模块测试）
cmake .. `
    -DBUILD_TESTS=ON `
    -DBUILD_VIDEO_PIPELINE_TESTS=ON `
    -DBUILD_PULLER_TESTS=ON `
    -DBUILD_DECODER_TESTS=ON `
    -DBUILD_PREPROCESS_TESTS=ON `
    -DBUILD_POSTPROCESS_TESTS=ON `
    -DBUILD_ALG_TESTS=ON

# 编译
cmake --build . --config Debug
```

### 方法 3: 只测试特定模块

```powershell
# 只测试 puller 模块
cmake .. -DBUILD_PULLER_TESTS=ON
cmake --build . --config Debug

# 运行测试
ctest -R test_puller -C Debug --verbose
```

## 🧪 运行测试

### 列出所有可用测试
```powershell
ctest -N
```

### 运行所有测试
```powershell
ctest -C Debug --verbose
```

### 运行特定模块的测试
```powershell
# Puller 测试
ctest -R test_puller -C Debug --verbose

# Decoder 测试
ctest -R test_decoder -C Debug --verbose

# VideoPipeline 测试
ctest -R test_videopipeline -C Debug --verbose

# Algorithm 测试
ctest -R test_alg -C Debug --verbose
```

## 📊 预期测试结果

### 成功的测试输出示例
```
Test project D:/file_mx/aaaaa/learncpp/build
    Start 1: test_puller_test_zlm_httpflv_puller
1/6 Test #1: test_puller_test_zlm_httpflv_puller .......   Passed    5.23 sec
    Start 2: test_decoder_test_ffmpeg_decoder
2/6 Test #2: test_decoder_test_ffmpeg_decoder ..........   Passed    3.45 sec
...

100% tests passed, 0 tests failed out of 6
```

### 常见问题

#### 1. 找不到头文件
**错误**: `fatal error: 'puller/zlm/zlm_httpflv_puller.h' file not found`

**解决**: 确保 CMake 配置时包含了正确的 include 路径
```cmake
target_include_directories(${PROJECT_NAME} PRIVATE include)
```

#### 2. 链接错误
**错误**: `unresolved external symbol`

**解决**: 检查是否正确链接了依赖库
- FFmpeg: `${FFMPEG_LIBRARIES}`
- OpenCV: `${OpenCV_LIBS}`
- gRPC: `grpc_lib`

#### 3. 测试可执行文件未生成
**原因**: 对应的 BUILD_*_TESTS 选项未启用

**解决**: 重新配置 CMake，启用相应选项
```powershell
cmake .. -DBUILD_PULLER_TESTS=ON
```

## 🔍 调试技巧

### 查看详细编译输出
```powershell
cmake --build . --config Debug --verbose
```

### 查看 CMake 变量
```powershell
cmake .. -LH
```

### 清理并重新构建
```powershell
Remove-Item -Recurse -Force build
mkdir build
cd build
cmake .. -DBUILD_TESTS=ON
cmake --build . --config Debug
```

## 📝 注意事项

1. **命名规范**: 所有 public 方法已改为首字母大写（PascalCase）
2. **命名空间**: 已移除 `video_pipeline` 命名空间，所有类在全局作用域
3. **头文件路径**: 已更新为新的目录结构
4. **类名变更**: 
   - `ZLMPuller` → `ZlmHttpFlvPuller`
   - `FFmpegDecoder` → `FfmpegDecoder`
   - `GrpcAlgorithmProcessor` → `GrpcToAlg`

## ✅ 验证清单

- [ ] Puller 模块编译通过
- [ ] Decoder 模块编译通过
- [ ] Preprocess 模块编译通过
- [ ] Postprocess 模块编译通过
- [ ] Alg 模块编译通过
- [ ] VideoPipeline 模块编译通过
- [ ] 所有测试可执行文件生成成功
- [ ] 测试能够正常运行（可能需要外部服务如 ZLMediaKit、gRPC server）

## 🎯 下一步

测试通过后，可以：
1. 集成到主程序中使用
2. 添加更多单元测试
3. 性能基准测试
4. 文档完善
