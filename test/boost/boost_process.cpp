//#define WIN32_LEAN_AND_MEAN
//#define NOMINMAX
//#include <winsock2.h>

#ifdef _WIN32
#include <WinSock2.h>
#pragma comment(lib, "ntdll.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#endif

//#include <boost/process/v1.hpp>
#include <boost/process.hpp>
#include <boost/process/windows/creation_flags.hpp>
#include <boost/process/windows/show_window.hpp>
#include <iostream>
#include <filesystem>
#include "zlmediakit/zlm_manager.h"
#include "common/log/logmanager.h"
namespace bp = boost::process;

std::string GetZlmPath() {
    std::filesystem::path exec_path;
#ifdef _WIN32
    exec_path = std::filesystem::path("tools") / "win32" / "zlmediakit" / "MediaServer.exe";
#else
    
#endif
	std::cout << "GetZlmPath, exec_path: " << exec_path.string() << std::endl;
    
    std::filesystem::path parent_path = std::filesystem::current_path().parent_path();
    exec_path = parent_path / exec_path;
    std::cout << "GetZlmPath, exec_path: " << exec_path.string() << std::endl;
    if (std::filesystem::exists(exec_path)) {
        return exec_path.string();
    }
    return "";
}

void testTerminal() {
    std::cout << "func testTerminal" << std::endl;
#ifdef _WIN32
    boost::asio::io_context ctx;
  boost::process::process proc(ctx,
    "c:\\windows\\system32\\ping.exe",
    { "www.google.com", "-n", "4" },
    boost::process::windows::create_new_console, boost::process::windows::show_window_maximized);
  proc.wait();
#endif
}

void testZlm() {
    std::string zlm_path = GetZlmPath();
    if (zlm_path.empty()) {
        std::cout << "Zlm path is empty!" << std::endl;
        return;
    }
    boost::asio::io_context ctx;
    boost::process::process proc(ctx,
        zlm_path,
        {},
        boost::process::windows::create_new_console, boost::process::windows::show_window_maximized);
    proc.wait();
    std::cout << "Zlm exited with code: " << proc.exit_code() << "\n";
}

void testZlmManager() {
    boost::asio::io_context ctx;
    ZLMProcessManager::Config cfg;
    cfg.debug_terminal = true;
    //ZLMProcessManager zlm_mgr(ctx, cfg);
    // 使用 shared_ptr 管理生命周期
    auto zlm_mgr = std::make_shared<ZLMProcessManager>(ctx, cfg);
    if (zlm_mgr->Start()) {
        std::cout << "ZLM Process started successfully." << std::endl;
        ctx.run();  // 让 io_context 运行，保持程序运行
    } else {
        std::cout << "Failed to start ZLM Process." << std::endl;
    }
}

int main() {
    LogManager& log_manager = LogManager::getInstance();
        log_manager.Init();
        std::cout << "LogManager initialized" << std::endl;
    /*boost::asio::io_context ctx;
    boost::process::process proc(ctx,
        "c:\\windows\\system32\\ping.exe",
        { "www.google.com", "-n", "4" });
    proc.wait();
    std::cout << proc.exit_code() << "\n";*/
    
    // boost::asio::io_context ctx;
    // boost::process::process proc(ctx,
    //     "c:\\windows\\system32\\cmd.exe",
    //     { "/C", "dir" });

    //testTerminal();
	//testZlm();
    testZlmManager();

    return 0;
}
