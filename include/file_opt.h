#include <string>
#include <vector>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
//#include <signal.h>
//#include <process.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

class FileOpt {
public:
	/// @brief 执行一个可执行程序
	/// @param program 可执行程序路径
	/// @param args 命令行参数
	/// @param hide_window 是否隐藏窗口
	/// @return 执行成功返回进程ID，否则返回-1
	static int Execute(const std::string& program, const std::vector<std::string>& args, bool hide_window);

private:
#ifdef _WIN32
	static std::vector<HANDLE> handles_;	// 存储创建的子进程句柄	
	static CRITICAL_SECTION cs_;	// 保护 handles_ 线程安全的互斥锁
	
	static bool initialized_;
	// 信号处理相关的函数
	static BOOL WINAPI CtrlHandler(DWORD fdwCtrlType);
	static void TerminateAllChildProcesses();	
	static void Initialize();
#endif

};

