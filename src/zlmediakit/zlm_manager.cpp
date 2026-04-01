#include <boost/process/v2/process.hpp>
#ifdef _WIN32
#include <boost/process/windows/creation_flags.hpp>
#include <boost/process/windows/show_window.hpp>
#endif
#include <filesystem>
#include <vector>
#include <iostream>
#include <cstdlib>

#include "zlmediakit/zlm_manager.h"
#include "log/logmanager.h"
#include "net/httprouter.h"

/// @brief 从 ZlmConfig 创建 ZLMProcessManager::Config
static ZLMProcessManager::Config createProcessConfig(const ZlmConfig& zlm_config) {
    ZLMProcessManager::Config cfg;
    cfg.debug_terminal = zlm_config.debug_terminal;
#ifdef _WIN32    
    cfg.work_dir = "./tools/win32/zlmediakit";
#else
    cfg.work_dir = "./tools/linux/zlmediakit";
#endif
    return cfg;
}

/// @brief 从 ZlmConfig 创建 ZLMAddressConfig
static ZLMAddressConfig createApiConfig(const ZlmConfig& zlm_config) {
    ZLMAddressConfig cfg;
    cfg.host = zlm_config.zlm_host;
    cfg.port = zlm_config.zlm_port;
    cfg.secret = zlm_config.secret;
    return cfg;
}


ZLMProcessManager::ZLMProcessManager(boost::asio::io_context& ctx, const Config& cfg)
    : ctx_(ctx), config_(cfg) {
    // 注册全局清理函数，确保程序退出时调用 Stop()
    std::atexit([]() {
        // 注意：atexit 中无法访问成员变量，所以这里不做具体操作
        // 实际清理在析构函数中进行
    });
}

ZLMProcessManager::~ZLMProcessManager() {    
	LOG_MAIN_INFO_AT("ZLMProcessManager: Destructor called, cleaning up...");
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
		LOG_MAIN_INFO_AT("ZLMProcessManager: Process exit callback triggered with code {}, error: {}", exit_code, ec.message());
        onProcessExit(exit_code, ec);
	});
	LOG_MAIN_INFO_AT("ZLMProcessManager: Started ZLMediaKit process with PID: {}", zlm_process_->id());
    return true;
}

void ZLMProcessManager::Stop() {
    // 防止重复进入
    ServiceStatus expected = ServiceStatus::STATUS_RUNNING;
    if (!status_.compare_exchange_strong(expected, ServiceStatus::STATUS_STOPPING)) {
        // 如果当前不是 RUNNING（可能是 STARTING/ERROR/STOPPING），也尝试清理，但跳过 terminate
        if (status_ == ServiceStatus::STATUS_STOPPED) {            
			LOG_MAIN_INFO_AT("ZLMProcessManager: Already stopped, skipping Stop()");
            return;
        }
    }
    
	LOG_MAIN_INFO_AT("ZLMProcessManager: Stopping...");

    status_ = ServiceStatus::STATUS_STOPPING;
    
    bool process_was_running = false;

    // 终止 ZLMediaKit 进程
    if (zlm_process_.has_value()) {
        try {
            if (zlm_process_->running()) {
                process_was_running = true;
                // 先尝试正常终止
                zlm_process_->terminate();                
				LOG_MAIN_INFO_AT("ZLMProcessManager: Sent terminate signal to zlm process with PID: {}", zlm_process_->id());
                
                // 等待一小段时间让进程自行退出
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                
                // 检查是否还在运行
                if (zlm_process_->running()) {                    
					LOG_MAIN_INFO_AT("ZLMProcessManager: Process still running after terminate, forcing kill");                    
#ifdef _WIN32
                    // 如果还在运行，强制结束（Windows 特有）
                    // Windows 下可以使用 taskkill 强制终止整个进程树
                    try {
                        auto pid = zlm_process_->id();
                        std::string cmd = "taskkill /F /PID " + std::to_string(pid);
                        std::system(cmd.c_str());                        
						LOG_MAIN_INFO_AT("ZLMProcessManager: Force killed zlm process with PID: {}", pid);
                    } catch (...) {                        
						LOG_MAIN_INFO_AT("ZLMProcessManager: Failed to force kill zlm process");
                    }
#endif
                }
                
                // 关键：同步等待进程真正退出，这样 async_wait 的回调才会被触发并返回
                try {
                    std::cout << "ZLMProcessManager: Waiting for process to exit..." << std::endl;
                    zlm_process_->wait();
					LOG_MAIN_INFO_AT("ZLMProcessManager: Process has exited with code: {}", zlm_process_->exit_code());
                } catch (const std::exception& e) {                    
					LOG_MAIN_ERROR_AT("ZLMProcessManager: Exception during wait: {}", e.what());
                }
            } else {
				LOG_MAIN_INFO_AT("ZLMProcessManager: Process was not running, skipping terminate");
            }
        } catch (const std::exception& e) {            
			LOG_MAIN_ERROR_AT("ZLMProcessManager: Exception during terminate: {}", e.what());
        } catch (...) {            
			LOG_MAIN_ERROR_AT("ZLMProcessManager: Unknown exception during terminate");
        }
        // 无论是否调用过 terminate，都 reset 掉 optional
        zlm_process_.reset();
    }

    status_ = ServiceStatus::STATUS_STOPPED;    
	LOG_MAIN_INFO_AT("ZLMProcessManager: Stopped");
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
    try {
#ifdef _WIN32
        zlm_process_.emplace(
            ctx_.get_executor(),  // executor_type
            zlm_path,             // 可执行文件路径
            std::vector<std::string>(),  // 参数列表，可以为空
            boost::process::windows::create_new_console,
		    boost::process::windows::show_window_normal
        );
#elif defined(__linux__)
        // Linux: 使用 xterm 或 gnome-terminal 等终端启动
        // 获取可执行文件名用于终端标题
        std::filesystem::path exe_path(zlm_path);
        std::string title = exe_path.filename().string();

        // 优先尝试 xterm，备选 gnome-terminal 或 konsole
        std::vector<std::string> terminal_args = {
            "-T", title,           // 终端标题
            "-e", zlm_path         // 执行的命令
        };

        // 尝试使用 xterm
        if (std::filesystem::exists("/usr/bin/xterm")) {
            zlm_process_.emplace(
                ctx_.get_executor(),
                "/usr/bin/xterm",
                terminal_args,
                bp2::process_start_dir(config_.work_dir)
            );
        }
        // 备选：gnome-terminal
        else if (std::filesystem::exists("/usr/bin/gnome-terminal")) {
            // gnome-terminal 参数格式不同
            std::vector<std::string> gnome_args = {
                "--title", title,
                "--", zlm_path
            };
            zlm_process_.emplace(
                ctx_.get_executor(),
                "/usr/bin/gnome-terminal",
                gnome_args,
                bp2::process_start_dir(config_.work_dir)
            );
        }
        // 备选：konsole (KDE)
        else if (std::filesystem::exists("/usr/bin/konsole")) {
            std::vector<std::string> konsole_args = {
                "--title", title,
                "-e", zlm_path
            };
            zlm_process_.emplace(
                ctx_.get_executor(),
                "/usr/bin/konsole",
                konsole_args,
                bp2::process_start_dir(config_.work_dir)
            );
        }
        else {
            std::cerr << "No suitable terminal emulator found for debug mode" << std::endl;
            return false;
        }
#else
#endif
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to start ZLM in debug mode: " << e.what() << std::endl;
        return false;
    }
    //zlm_process_.value().wait();
	return true;
}

bool ZLMProcessManager::startZLMProcessNormal() {
    std::string zlm_path = GetZlmediakitPath();
    if (zlm_path.empty()) {
       return false;
    }

    zlm_process_.emplace(
        ctx_.get_executor(),
        zlm_path,
        std::vector<std::string>{},
		boost::process::process_start_dir{ config_.work_dir }
    );

    return true;
}

void ZLMProcessManager::onProcessExit(int exit_code, const std::error_code& ec) {
    // 立即将进程对象从成员变量中移出，防止析构时再次操作
    // 使用临时变量持有，确保本函数执行期间 process 对象仍然有效
    auto proc = std::move(zlm_process_);

    try {        
        if (ec.value() == boost::asio::error::operation_aborted) {
			LOG_MAIN_INFO_AT("ZLMProcessManager: Process wait operation was aborted, likely due to Stop() being called");
            return;
        }

        // 检查当前状态，如果正在停止或已经停止，则不再处理退出事件
        ServiceStatus current_status = status_.load();
        if (current_status == ServiceStatus::STATUS_STOPPING || current_status == ServiceStatus::STATUS_STOPPED) {            
			LOG_MAIN_INFO_AT("ZLMProcessManager: Manager is stopping/stopped, skipping exit handler");
            return;
        }

        if (ec) {
            LOG_MAIN_ERROR_AT("ZLM process exited with error: {}", ec.message());            
            status_ = ServiceStatus::STATUS_ERROR;
            return;
        }

        if (exit_code != 0) {
			LOG_MAIN_ERROR_AT("ZLM process exited with non-zero code: {}", exit_code);
            status_ = ServiceStatus::STATUS_ERROR;
            return;
        } else {            
			LOG_MAIN_INFO_AT("ZLM process exited normally with code 0");
            status_ = ServiceStatus::STATUS_STOPPED;
        }
    } catch (const std::exception& e) {        
		LOG_MAIN_ERROR_AT("ZLMProcessManager: Exception during wait: {}", e.what());
        status_ = ServiceStatus::STATUS_ERROR;
    } catch (...) {        
		LOG_MAIN_ERROR_AT("ZLMProcessManager: Unknown exception during wait");
        status_ = ServiceStatus::STATUS_ERROR;
    }
    //TODO: 重启 ZLMediaKit 进程
}

bool ZLMManager::Start()
{
    // 注册 Hook 路由
    RegisterRoutes();
    
    // 启动 ZLMediaKit 进程
    if (!process_.Start()) {
        LOG_MAIN_CRITICAL_AT("ZLMManager: Failed to start ZLM process");
        return false;
    }
    
    LOG_MAIN_INFO_AT("ZLMManager: Started successfully");
    return true;
}

void ZLMManager::Stop() {
    LOG_MAIN_INFO_AT("ZLMManager: Stopping...");
    
    // 1. 停止进程
    process_.Stop();
    
    LOG_MAIN_INFO_AT("ZLMManager: Stopped");
}

ZLMManager::ZLMManager(boost::asio::io_context& ctx, 
                       Net::HttpClientPool* pool,
                       const ZlmConfig& zlm_config)
    : ctx_(ctx)
    , zlm_config_(zlm_config)
    , process_(ctx, createProcessConfig(zlm_config))
    , api_client_(ctx, pool, createApiConfig(zlm_config))
    , hook_handler_(nullptr) {
    
    LOG_MAIN_INFO_AT("ZLMManager: Initialized with config - host: {}, port: {}, secret: {}",
                    zlm_config.zlm_host, zlm_config.zlm_port, zlm_config.secret);
}

ZLMManager::~ZLMManager() {
    LOG_MAIN_INFO_AT("ZLMManager: Destructor called");
    Stop();
}

void ZLMManager::RegisterRoutes() {
    // 使用配置文件中的 secret
    hook_handler_ = std::make_unique<ZLMHookHandler>(zlm_config_.secret);
    if (hook_handler_) {
        hook_handler_->RegisterRoutes();
        LOG_MAIN_INFO_AT("ZLMManager: Hook routes registered with secret: {}", zlm_config_.secret);
    } else {
        LOG_MAIN_ERROR_AT("ZLMManager: Failed to create ZLMHookHandler");
    }
}
