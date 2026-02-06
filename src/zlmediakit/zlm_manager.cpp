#include <boost/process/windows/creation_flags.hpp>
#include <boost/process/windows//show_window.hpp>
#include <filesystem>
#include <vector>
#include <iostream>

#include "zlmediakit/zlm_manager.h"
#include "log/logmanager.h"
#include "net/httprouter.h"
#include "common/errcode.h"

ZLMProcessManager::ZLMProcessManager(boost::asio::io_context& ctx, const Config& cfg)
    : ctx_(ctx), config_(std::move(cfg)) {
}

ZLMProcessManager::~ZLMProcessManager() {
    Stop();
}

std::string ZLMProcessManager::GetZlmediakitPath() {
    std::filesystem::path exec_path;
#ifdef _WIN32
    exec_path = std::filesystem::path("tools") / "win32" / "zlmediakit" / "MediaServer.exe";
#else
    exec_path = std::filesystem::path("tools") / "linux" / "zlmediakit" / "MediaServer";
#endif    

    std::filesystem::path parent_path = std::filesystem::current_path().parent_path();
    exec_path = parent_path / exec_path; 
    if (std::filesystem::exists(exec_path)) {
        return exec_path.string();
    }
    return "";
}

bool ZLMProcessManager::Start() {
    if (status_ != ServiceStatus::STATUS_STOPPED) {
        return false;
    }

    status_ = ServiceStatus::STATUS_STARTING;

    bool ok = false;
    if (config_.debug_terminal) {
		ok = startZLMProcessDebug();
    }
    else {
        ok = startZLMProcessNormal();
    }

	if (!ok || !zlm_process_.has_value()) {
        status_ = ServiceStatus::STATUS_ERROR;
        return false;
    }

    status_ = ServiceStatus::STATUS_RUNNING;

    zlm_process_->async_wait([this](const std::error_code& ec, int exit_code) {
        std::cout << "ZLMProcessManager: Process exit callback triggered" << std::endl;
        onProcessExit(exit_code, ec);
	});
    std::cout << "ZLM process started" << std::endl;
    return true;
}

void ZLMProcessManager::Stop() {
    // 防止重复进入
    ServiceStatus expected = ServiceStatus::STATUS_RUNNING;
    if (!status_.compare_exchange_strong(expected, ServiceStatus::STATUS_STOPPING)) {
        // 如果当前不是 RUNNING（可能是 STARTING/ERROR/STOPPING），也尝试清理，但跳过 terminate
        if (status_ == ServiceStatus::STATUS_STOPPED) {
            return;
        }
    }

    std::cout << "ZLMProcessManager: Stopping..." << std::endl;

    status_ = ServiceStatus::STATUS_STOPPING;
    

    // 终止 ZLMediaKit 进程
    if (zlm_process_.has_value()) {
        try {
            if (zlm_process_->running()) {
                zlm_process_->terminate();
                std::cout << "ZLMProcessManager: Process terminated" << std::endl;
            } else {
                std::cout << "ZLMProcessManager: Process is not running" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "ZLMProcessManager: Exception during terminate: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "ZLMProcessManager: Unknown exception during terminate" << std::endl;
        }
        // 无论是否调用过 terminate，都 reset 掉 optional
        zlm_process_.reset();
    }

    status_ = ServiceStatus::STATUS_STOPPED;
}

bool ZLMProcessManager::IsRunning() const {
	return status_ == ServiceStatus::STATUS_RUNNING;
}

ServiceStatus ZLMProcessManager::GetStatus() const {
	return status_;
}

bool ZLMProcessManager::startZLMProcessDebug() {
    std::string zlm_path = GetZlmediakitPath();
    
    if (zlm_path.empty()) {        
        return false;
    }
#ifdef _WIN32
    zlm_process_.emplace(
        ctx_.get_executor(),  // executor_type
        zlm_path,             // 可执行文件路径
        std::vector<std::string>(),  // 参数列表，可以为空
        boost::process::windows::create_new_console,
		boost::process::windows::show_window_normal
    );
#elif defined(__linux__)
    //zlm_process_ = std::make_unique<bp::process>(
    //    io_,
    //    "xterm",
    //    std::vector<std::string>{
    //    "-T", "ZLMediaKit",
    //        "-e", cfg_.zlm_executable_path
    //},
    //    bp::process_start_dir{ cfg_.work_dir }
    //);
#else
#endif
    //zlm_process_.value().wait();
	return true;
}

bool ZLMProcessManager::startZLMProcessNormal() {
    std::string zlm_path = GetZlmediakitPath();
    if (zlm_path.empty()) {
       return false;
    }

    //zlm_process_.emplace(
    //    ctx_.get_executor(),
    //    zlm_path,
    //    std::vector<std::string>{},
    //    boost::process::start_dir{ config_.work_dir },
    //    boost::process::std_out > boost::process::null,
    //    boost::process::std_err > boost::process::null
    //);

    return true;
}

void ZLMProcessManager::onProcessExit(int exit_code, const std::error_code& ec) {
    // 立即将进程对象从成员变量中移出，防止析构时再次操作
    // 使用临时变量持有，确保本函数执行期间 process 对象仍然有效
    auto proc = std::move(zlm_process_);

    try {
        std::cout << "ZLMProcessManager: Process exited with code " << exit_code << std::endl;
        if (ec.value() == boost::asio::error::operation_aborted) {
            std::cout << "ZLMProcessManager: Process wait cancelled (normal shutdown)" << std::endl;
            return;
        }

        // 检查当前状态，如果正在停止或已经停止，则不再处理退出事件
        ServiceStatus current_status = status_.load();
        if (current_status == ServiceStatus::STATUS_STOPPING || current_status == ServiceStatus::STATUS_STOPPED) {
            std::cout << "ZLMProcessManager: Manager is stopping/stopped, skipping exit handler" << std::endl;
            return;
        }

        if (ec) {
            //LOG_MAIN_ERROR_AT("ZLM process exited with error: {}", ec.message());
            std::cerr << "ZLMProcessManager: Exception during wait: " << ec.message() << std::endl;
            status_ = ServiceStatus::STATUS_ERROR;
            return;
        }

        if (exit_code != 0) {
            std::cerr << "ZLMProcessManager: Process exited with non-zero code" << std::endl;
            status_ = ServiceStatus::STATUS_ERROR;
            return;
        } else {
            std::cout << "ZLMProcessManager: Process exited normally, code is zero" << std::endl;
            status_ = ServiceStatus::STATUS_STOPPED;
        }
    } catch (const std::exception& e) {
        std::cerr << "ZLMProcessManager: Exception during wait: " << e.what() << std::endl;
        status_ = ServiceStatus::STATUS_ERROR;
    } catch (...) {
        std::cerr << "ZLMProcessManager: Unknown exception during wait" << std::endl;
        status_ = ServiceStatus::STATUS_ERROR;
    }
    //TODO: 重启 ZLMediaKit 进程
}

bool ZLMManager::Start()
{
    RegisterRoutes();

    // 启动 ZLMediaKit 进程
    return process_.Start();

}

void ZLMManager::RegisterRoutes() {
    std::string zlm_key = "zlmediakit"; // 假设这是从配置文件或环境变量中获取的

    hook_handler_ = ZLMHookHandler { zlm_key };   
    hook_handler_.RegisterRoutes();
}
