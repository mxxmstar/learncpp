# Decoder 和 Preprocess 测试文件迁移完成报告

## 概述

成功将 decoder 和 preprocess 模块的测试文件迁移到对应的 modules 目录，并启用了测试编译选项。

## 迁移的测试文件

### 1. Decoder 模块测试 🧪

**测试文件：**
```
modules/decoder/test/
├── CMakeLists.txt                    ← 新创建
└── test_ffmpeg_decoder.cpp (8.4 KB)  ← 从 test/decoder/ 迁移
```

**测试配置：**
```cmake
# modules/decoder/test/CMakeLists.txt
target_link_libraries(test_decoder_test_ffmpeg_decoder
    PRIVATE
        decoder_lib
        log_lib
)
```

**测试开关：**
```cmake
option(BUILD_DECODER_TESTS "Build decoder module tests" ON)  # ✅ 已启用
```

---

### 2. Preprocess 模块测试 🧪

**测试文件：**
```
modules/preprocess/test/
├── CMakeLists.txt                              ← 新创建
└── format_converter/
    └── test_format_converter.cpp (2.1 KB)      ← 从 test/preprocess/format_converter/ 迁移
```

**测试配置：**
```cmake
# modules/preprocess/test/CMakeLists.txt
target_link_libraries(test_preprocess_format_converter_test_format_converter
    PRIVATE
        preprocess_lib
        decoder_lib  # preprocess 依赖 decoder
        log_lib
)
```

**测试开关：**
```cmake
option(BUILD_PREPROCESS_TESTS "Build preprocess module tests" ON)  # ✅ 已启用
```

---

## 已启用测试的模块

| 模块 | 测试开关 | 测试文件数 | 状态 |
|------|----------|-----------|------|
| **puller** | `BUILD_PULLER_TESTS = ON` | 1 | ✅ |
| **decoder** | `BUILD_DECODER_TESTS = ON` | 1 | ✅ |
| **preprocess** | `BUILD_PREPROCESS_TESTS = ON` | 1 | ✅ |

---

## CMake 配置详情

### Decoder 测试

```cmake
# modules/decoder/test/CMakeLists.txt

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

file(GLOB DECODER_TEST_SOURCES "*.cpp")

foreach(test_file ${DECODER_TEST_SOURCES})
    get_filename_component(test_name ${test_file} NAME_WE)
    set(test_target "test_decoder_${test_name}")
    
    add_executable(${test_target} ${test_file})
    
    target_include_directories(${test_target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    
    target_link_libraries(${test_target}
        PRIVATE
            decoder_lib
            log_lib
    )
    
    add_test(NAME ${test_target} COMMAND ${test_target})
    message(STATUS "Added decoder test: ${test_target}")
endforeach()
```

### Preprocess 测试

```cmake
# modules/preprocess/test/CMakeLists.txt

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

# 使用 GLOB_RECURSE 支持子目录
file(GLOB_RECURSE PREPROCESS_TEST_SOURCES "*.cpp")

foreach(test_file ${PREPROCESS_TEST_SOURCES})
    # 获取相对路径作为测试名称
    file(RELATIVE_PATH rel_path ${CMAKE_CURRENT_SOURCE_DIR} ${test_file})
    string(REPLACE "/" "_" test_name ${rel_path})
    string(REPLACE ".cpp" "" test_name ${test_name})
    set(test_target "test_preprocess_${test_name}")
    
    add_executable(${test_target} ${test_file})
    
    target_include_directories(${test_target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    
    target_link_libraries(${test_target}
        PRIVATE
            preprocess_lib
            decoder_lib  # 注意：preprocess 依赖 decoder
            log_lib
    )
    
    add_test(NAME ${test_target} COMMAND ${test_target})
    message(STATUS "Added preprocess test: ${test_target}")
endforeach()
```

---

## 测试可执行文件输出

测试编译后，可执行文件将输出到各模块的 `test/bin/` 目录：

```
modules/
├── decoder/
│   └── test/
│       └── bin/
│           └── test_decoder_test_ffmpeg_decoder.exe
├── preprocess/
│   └── test/
│       └── bin/
│           └── test_preprocess_format_converter_test_format_converter.exe
└── puller/
    └── test/
        └── bin/
            └── test_puller_test_zlm_httpflv_puller.exe
```

---

## 运行测试

### 方法 1：通过 CTest

```powershell
cd out\build\x64-debug
ctest -V  # 详细输出
```

### 方法 2：直接运行测试可执行文件

```powershell
# Decoder 测试
.\modules\decoder\test\bin\test_decoder_test_ffmpeg_decoder.exe

# Preprocess 测试
.\modules\preprocess\test\bin\test_preprocess_format_converter_test_format_converter.exe

# Puller 测试
.\modules\puller\test\bin\test_puller_test_zlm_httpflv_puller.exe
```

### 方法 3：在 Visual Studio 中

1. 打开 **测试资源管理器** (Test → Test Explorer)
2. 点击 **运行所有测试** (Run All Tests)

---

## 修改的文件清单

### 新建文件
1. ✅ `modules/decoder/test/CMakeLists.txt`
2. ✅ `modules/preprocess/test/CMakeLists.txt`

### 复制文件
3. ✅ `test/decoder/test_ffmpeg_decoder.cpp` → `modules/decoder/test/`
4. ✅ `test/preprocess/format_converter/test_format_converter.cpp` → `modules/preprocess/test/format_converter/`

### 修改文件
5. ✅ `modules/decoder/CMakeLists.txt` - 启用 BUILD_DECODER_TESTS
6. ✅ `modules/preprocess/CMakeLists.txt` - 启用 BUILD_PREPROCESS_TESTS

---

## 验证步骤

### 1. 重新配置 CMake

在 Visual Studio 中：
```
Project → Delete Cache and Reconfigure
```

应该看到以下消息：
```
-- Added decoder test: test_decoder_test_ffmpeg_decoder
-- Added preprocess test: test_preprocess_format_converter_test_format_converter
-- Added puller test: test_puller_test_zlm_httpflv_puller
```

### 2. 编译项目

```
Build → Build All
```

应该编译出 3 个测试可执行文件。

### 3. 运行测试

在 Visual Studio 中：
```
Test → Run → All Tests
```

或者命令行：
```powershell
cd out\build\x64-debug
ctest --output-on-failure
```

---

## 注意事项

1. **Preprocess 测试依赖 Decoder**
   - preprocess 的测试需要链接 `decoder_lib`
   - 因为使用了 `VideoFrame` 结构

2. **测试输出目录**
   - 每个模块的测试输出到各自的 `test/bin/` 目录
   - 便于管理和清理

3. **测试命名规范**
   - 格式：`test_<module>_<test_name>`
   - 例如：`test_decoder_test_ffmpeg_decoder`

4. **子目录支持**
   - preprocess 测试使用 `GLOB_RECURSE` 支持子目录
   - 测试名称包含路径信息（用 `_` 替换 `/`）

---

## 后续工作

### 添加更多测试

可以为其他模块添加测试：
- camera 模块测试
- web 模块测试
- zlmediakit 模块测试

### 集成 CI/CD

在持续集成中添加测试步骤：

```yaml
- name: Run Tests
  run: |
    cd build
    ctest --output-on-failure
```

### 测试覆盖率

使用工具如 gcov/lcov 或 OpenCppCoverage 生成测试覆盖率报告。

---

## 状态

✅ **Decoder 测试文件迁移完成**
✅ **Preprocess 测试文件迁移完成**
✅ **测试编译选项已启用**
✅ **CMake 配置已完成**

可以进行编译和测试验证了！🎉
