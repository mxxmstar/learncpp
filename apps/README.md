# Apps 目录

本目录包含应用程序入口点。

## 📁 文件说明

- `main.cpp` - 主程序入口

## 🔧 编译

主程序通过根目录的 CMakeLists.txt 编译：

```cmake
add_executable(${PROJECT_NAME} apps/main.cpp)
```

## 📝 未来扩展

当需要添加其他应用程序时，可以在此目录下创建：

```
apps/
├── main.cpp              # 主程序
├── test_app.cpp          # 测试应用
├── demo.cpp              # 演示程序
└── benchmark.cpp         # 性能测试
```

每个应用都可以在 CMakeLists.txt 中独立配置：

```cmake
add_executable(test_app apps/test_app.cpp)
target_link_libraries(test_app PRIVATE log_lib net_lib ...)
```

## 💡 设计原则

- **apps/** = 应用程序（可执行文件）
- **modules/** = 库模块（静态/动态库）
- **examples/** = 示例代码（可选，用于演示如何使用模块）

这种结构清晰地区分了"可复用的库"和"使用库的应用"。
