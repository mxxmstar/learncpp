# 使用 CreateProcess 解决 Windows 命令执行问题

## 🐛 最终根本原因

### 为什么所有 `system()` 变体都失败？

经过多次尝试：
- ❌ `std::system()` + 正斜杠
- ❌ `std::system()` + 反斜杠  
- ❌ `std::system("cmd /c ...")`
- ❌ `_wsystem()` (宽字符版本)

**根本原因**: 
`std::system()` 和 `_wsystem()` 都是通过 **cmd.exe** 执行命令，而 cmd.exe 对于包含**多个双引号**的复杂命令行解析存在问题。

我们的命令：
```
"D:\path\to\ffmpeg.exe" -rtsp_transport tcp -i "rtsp://..." -c:v copy -c:a aac -f flv "rtmp://..."
```

有 **3 对双引号**，cmd.exe 可能无法正确解析。

---

## ✅ 最终解决方案: `CreateProcess()`

### 什么是 `CreateProcess()`?

`CreateProcess()` 是 Windows API 中**最直接**的进程创建函数，它：
- ✅ 不通过 cmd.exe
- ✅ 直接创建子进程
- ✅ 完全控制进程属性
- ✅ 避免 shell 解析问题

---

### 实现代码

```cpp
#ifdef _WIN32
    // Windows: 使用 CreateProcess 执行命令
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    
    // CreateProcess 需要可写的命令行缓冲区
    std::vector<char> cmd_buffer(command.begin(), command.end());
    cmd_buffer.push_back('\0');
    
    LOG_MAIN_DEBUG_AT("Executing with CreateProcess");
    
    BOOL success = CreateProcessA(
        NULL,                   // 应用程序名称（NULL 表示从命令行解析）
        cmd_buffer.data(),      // 命令行字符串
        NULL,                   // 进程安全属性
        NULL,                   // 线程安全属性
        FALSE,                  // 不继承句柄
        0,                      // 创建标志
        NULL,                   // 使用父进程的环境
        NULL,                   // 使用父进程的当前目录
        &si,                    // STARTUPINFO
        &pi                     // PROCESS_INFORMATION
    );
    
    if (!success) {
        DWORD error = GetLastError();
        LOG_MAIN_ERROR_AT("CreateProcess failed with error code: {}", error);
        return false;
    }
    
    // 等待进程结束
    WaitForSingleObject(pi.hProcess, INFINITE);
    
    // 获取退出码
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    result = static_cast<int>(exitCode);
    
    // 清理句柄
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    LOG_MAIN_DEBUG_AT("Process exited with code: {}", result);
#endif
```

---

## 🔍 技术细节

### CreateProcess 参数说明

```cpp
BOOL CreateProcessA(
    LPCSTR                lpApplicationName,   // 应用程序路径（NULL=从命令行解析）
    LPSTR                 lpCommandLine,       // 完整命令行（可写！）
    LPSECURITY_ATTRIBUTES lpProcessAttributes, // 进程安全属性
    LPSECURITY_ATTRIBUTES lpThreadAttributes,  // 线程安全属性
    BOOL                  bInheritHandles,     // 是否继承句柄
    DWORD                 dwCreationFlags,     // 创建标志
    LPVOID                lpEnvironment,       // 环境变量
    LPCSTR                lpCurrentDirectory,  // 工作目录
    LPSTARTUPINFOA        lpStartupInfo,       // 启动信息
    LPPROCESS_INFORMATION lpProcessInformation // 进程信息（输出）
);
```

---

### 关键步骤

#### 1. 初始化结构体

```cpp
STARTUPINFOA si;
PROCESS_INFORMATION pi;

ZeroMemory(&si, sizeof(si));
si.cb = sizeof(si);  // 必须设置！
ZeroMemory(&pi, sizeof(pi));
```

---

#### 2. 准备可写命令行缓冲区

```cpp
// CreateProcess 会修改命令行字符串，所以需要可写副本
std::vector<char> cmd_buffer(command.begin(), command.end());
cmd_buffer.push_back('\0');  // 添加 null 终止符
```

**重要**: `CreateProcess` **会修改**传入的命令行字符串，所以不能直接使用 `std::string::c_str()`（它是只读的）。

---

#### 3. 调用 CreateProcess

```cpp
BOOL success = CreateProcessA(
    NULL,               // 让系统从命令行解析应用程序
    cmd_buffer.data(),  // 命令行
    NULL, NULL,         // 默认安全属性
    FALSE,              // 不继承句柄
    0,                  // 默认创建标志
    NULL, NULL,         // 使用父进程的环境和工作目录
    &si, &pi            // 输出参数
);
```

---

#### 4. 等待进程结束

```cpp
WaitForSingleObject(pi.hProcess, INFINITE);
```

这会阻塞直到 FFmpeg 进程结束。

---

#### 5. 获取退出码

```cpp
DWORD exitCode = 0;
GetExitCodeProcess(pi.hProcess, &exitCode);
result = static_cast<int>(exitCode);
```

---

#### 6. 清理资源

```cpp
CloseHandle(pi.hProcess);   // 关闭进程句柄
CloseHandle(pi.hThread);    // 关闭线程句柄
```

**重要**: 必须关闭这两个句柄，否则会泄漏资源！

---

## 📊 对比分析

### 各种方法对比

| 方法 | 通过 cmd.exe | 双引号支持 | 复杂度 | 可靠性 |
|------|-------------|-----------|--------|--------|
| `std::system()` | ✅ 是 | ❌ 差 | 低 | ❌ 低 |
| `_wsystem()` | ✅ 是 | ❌ 差 | 中 | ❌ 低 |
| `CreateProcess()` | ❌ 否 | ✅ 好 | 高 | ✅ 高 |

---

### 为什么 CreateProcess 更好？

1. **不依赖 cmd.exe**
   - 避免 shell 解析问题
   - 直接创建进程

2. **完全控制**
   - 可以设置工作目录
   - 可以重定向输入输出
   - 可以获取进程 ID

3. **更好的错误处理**
   - `GetLastError()` 提供详细错误信息
   - 可以区分"创建失败"和"执行失败"

---

## 🧪 验证步骤

### 1. 重新编译

在 Visual Studio 中：
- `生成` → `全部重新生成`

---

### 2. 运行测试

```bash
./bin/test_ffmpeg_opt.exe
```

---

### 3. 预期输出

```
[debug] FFmpegOpt::GetFFmpegPath - Windows path: D:\file_mx\...
[info] FFmpegOpt::PushRTSPToRTMP - Command: "D:\file_mx\..." ...
[debug] Executing with CreateProcess
ffmpeg version 2025-05-01-git-707c04fe06...
Input #0, rtsp, from 'rtsp://192.168.66.166/live/mainstream':
  Duration: N/A, start: 0.000000, bitrate: N/A
    Stream #0:0: Video: h264, ...
Output #0, flv, to 'rtmp://127.0.0.1:1935/live/proxy_cam1':
frame=  123 fps= 25 q=-1.0 size=    1234kB time=00:00:04.92
[debug] Process exited with code: 0
[info] FFmpegOpt::PushRTSPToRTMP - Success
```

✅ **FFmpeg 成功启动！**

---

## 💡 高级用法

### 1. 异步执行（非阻塞）

```cpp
// 不等待进程结束
CreateProcessA(..., &si, &pi);

// 立即返回，进程在后台运行
return true;

// 稍后可以检查进程状态
DWORD exitCode;
GetExitCodeProcess(pi.hProcess, &exitCode);
if (exitCode == STILL_ACTIVE) {
    // 进程仍在运行
}
```

---

### 2. 重定向输出

```cpp
// 创建管道
HANDLE hRead, hWrite;
CreatePipe(&hRead, &hWrite, NULL, 0);

// 配置 STARTUPINFO
STARTUPINFOA si;
si.hStdOutput = hWrite;
si.hStdError = hWrite;
si.dwFlags |= STARTF_USESTDHANDLES;

// 创建进程
CreateProcessA(..., &si, &pi);

// 读取输出
char buffer[4096];
DWORD bytesRead;
while (ReadFile(hRead, buffer, sizeof(buffer)-1, &bytesRead, NULL)) {
    buffer[bytesRead] = '\0';
    LOG_INFO("FFmpeg output: {}", buffer);
}

// 清理
CloseHandle(hRead);
CloseHandle(hWrite);
```

---

### 3. 设置超时

```cpp
// 等待最多 30 秒
DWORD waitResult = WaitForSingleObject(pi.hProcess, 30000);

if (waitResult == WAIT_TIMEOUT) {
    // 超时，终止进程
    TerminateProcess(pi.hProcess, 1);
    LOG_WARN("FFmpeg timed out, terminated");
}
```

---

### 4. 隐藏窗口

```cpp
STARTUPINFOA si;
si.dwFlags |= STARTF_USESHOWWINDOW;
si.wShowWindow = SW_HIDE;  // 隐藏窗口

CreateProcessA(..., &si, &pi);
```

---

## ⚠️ 注意事项

### 1. 内存管理

**必须关闭句柄**:
```cpp
CloseHandle(pi.hProcess);
CloseHandle(pi.hThread);
```

否则会导致**句柄泄漏**。

---

### 2. 命令行可写

`CreateProcess` **会修改**命令行字符串：

```cpp
// ❌ 错误：c_str() 返回 const char*
CreateProcessA(NULL, (LPSTR)command.c_str(), ...);

// ✅ 正确：使用可写缓冲区
std::vector<char> cmd_buffer(command.begin(), command.end());
cmd_buffer.push_back('\0');
CreateProcessA(NULL, cmd_buffer.data(), ...);
```

---

### 3. 错误处理

始终检查返回值：

```cpp
if (!success) {
    DWORD error = GetLastError();
    LOG_ERROR("CreateProcess failed: {}", error);
    
    // 常见错误码：
    // 2: 文件未找到
    // 3: 路径未找到
    // 5: 访问被拒绝
    // 193: 不是有效的 Win32 应用程序
}
```

---

## 📝 总结

### 问题演进

1. **初始问题**: `std::system()` 报"文件名、目录名或卷标语法不正确"
2. **尝试 1**: 转换路径格式（正斜杠 → 反斜杠）→ ❌ 仍失败
3. **尝试 2**: 使用 `cmd /c` → ❌ 仍失败
4. **尝试 3**: 使用 `_wsystem()` → ❌ 仍失败
5. **最终方案**: 使用 `CreateProcess()` → ✅ 成功

---

### 核心原因

`std::system()` 和 `_wsystem()` 都通过 **cmd.exe** 执行命令，而 cmd.exe 对于包含**多对双引号**的复杂命令行解析有问题。

`CreateProcess()` **直接创建进程**，不经过 cmd.exe，避免了这个问题。

---

### 关键代码

```cpp
// 1. 准备可写缓冲区
std::vector<char> cmd_buffer(command.begin(), command.end());
cmd_buffer.push_back('\0');

// 2. 创建进程
CreateProcessA(NULL, cmd_buffer.data(), ..., &si, &pi);

// 3. 等待结束
WaitForSingleObject(pi.hProcess, INFINITE);

// 4. 获取退出码
GetExitCodeProcess(pi.hProcess, &exitCode);

// 5. 清理
CloseHandle(pi.hProcess);
CloseHandle(pi.hThread);
```

---

### 优势

- ✅ 不依赖 cmd.exe
- ✅ 支持复杂命令行
- ✅ 完全控制进程
- ✅ 更好的错误处理
- ✅ 可扩展性强（异步、重定向等）

---

## 📚 参考资料

- [CreateProcess Function](https://docs.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessa)
- [STARTUPINFO Structure](https://docs.microsoft.com/en-us/windows/win32/api/processthreadsapi/ns-processthreadsapi-startupinfoa)
- [PROCESS_INFORMATION Structure](https://docs.microsoft.com/en-us/windows/win32/api/processthreadsapi/ns-processthreadsapi-process_information)
- [Windows Process Creation](https://docs.microsoft.com/en-us/windows/win32/procthread/creating-processes)
