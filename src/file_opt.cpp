#include "file_opt.h"
#include <algorithm>


#ifdef _WIN32
std::vector<HANDLE> FileOpt::handles_{};
CRITICAL_SECTION FileOpt::cs_{};
bool FileOpt::initialized_ = false;

BOOL WINAPI FileOpt::CtrlHandler(DWORD fdwCtrlType) {
    switch (fdwCtrlType) {
        case CTRL_C_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_BREAK_EVENT:
            // 终止所有子进程
            FileOpt::TerminateAllChildProcesses();
            return TRUE;
        default:
            return FALSE;
    }
}

void FileOpt::TerminateAllChildProcesses() {
    EnterCriticalSection(&cs_);
    std::vector<HANDLE> temp_handles = handles_;
    LeaveCriticalSection(&cs_);
    
    for (HANDLE hProcess : temp_handles) {
        if (hProcess != nullptr) {
            DWORD exit_code;
            GetExitCodeProcess(hProcess, &exit_code);
            if (exit_code == STILL_ACTIVE) {
                TerminateProcess(hProcess, 1);
                WaitForSingleObject(hProcess, 1000); // 等待最多1秒
            }
            CloseHandle(hProcess);
        }
    }
    
    // 清空句柄列表
    EnterCriticalSection(&cs_);
    handles_.clear();
    LeaveCriticalSection(&cs_);
}

void FileOpt::Initialize() {
    if (!initialized_) {
        InitializeCriticalSection(&cs_);
        SetConsoleCtrlHandler(CtrlHandler, TRUE);
        initialized_ = true;
    }
}
#endif

int FileOpt::Execute(const std::string& program, const std::vector<std::string>& args, bool hide_window) {
#ifdef _WIN32	
    Initialize();   // 初始化互斥锁和信号处理

	std::string cmd = "\"" + program + "\"";
	for (const auto& arg : args) {
		cmd += " \"" + arg + "\"";
	}
	STARTUPINFOA si;
	PROCESS_INFORMATION pi;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	si.wShowWindow = hide_window ? SW_HIDE : SW_SHOW;
	ZeroMemory(&pi, sizeof(pi));
	
	// 注意：CreateProcess 要求 lpCommandLine 是可修改的 char 数组
    std::vector<char> cmdLine(cmd.begin(), cmd.end());
    cmdLine.push_back('\0');

	if (!CreateProcessA(
		nullptr,	// lpApplicationName
		cmdLine.data(),	// lpCommandLine
		nullptr,	// lpProcessAttributes
		nullptr,	// lpThreadAttributes
		FALSE,	// bInheritHandles
		0,	// dwCreationFlags
		nullptr,	// lpEnvironment
		nullptr,	// lpCurrentDirectory
		&si,	// lpStartupInfo
		&pi	// lpProcessInformation
	)) {
		return -1;
	}

	// 存储子进程句柄
	EnterCriticalSection(&cs_);
	handles_.push_back(pi.hProcess);
	LeaveCriticalSection(&cs_);

	WaitForSingleObject(pi.hProcess, INFINITE);

	// 从句柄列表中移除已结束的进程句柄
	EnterCriticalSection(&cs_);
	handles_.erase(std::remove(handles_.begin(), handles_.end(), pi.hProcess), handles_.end());
	LeaveCriticalSection(&cs_);
	
	// 获取进程退出码
	DWORD exit_code;
	GetExitCodeProcess(pi.hProcess, &exit_code);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	return static_cast<int>(exit_code);
#else
	pid_t pid = fork();
	if (pid == 0) {
		// 子进程执行
		std::vector<const char*> argv;
		argv.push_back(const_cast<char*>(program.c_str()));
		for (const auto& arg : args) {
			argv.push_back(arg.c_str());
		}
		argv.push_back(nullptr);	// execv 要求以 nullptr 结尾
		
		execv(program.c_str(), const_cast<char* const*>(argv.data()));
		_exit(127);	// execv 失败返回 127, 命令未找到
	} else {
		// 父进程等待子进程结束
		int status;
		waitpid(pid, &status, 0);
		if (WIFEXITED(status)) {
			return WEXITSTATUS(status);
		} else if (WIFSIGNALED(status)) {
			return -WTERMSIG(status);
		} else {
			return -1;
		}		
	}
#endif
}
