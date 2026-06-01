#include <filesystem>

#include "zlmediakit/zlm_manager.h"
#include "common/log/logmanager.h"
#include "net/http_server/http_router.h"

namespace process = common::process;

/// @brief 从 ZlmConfig 创建 ZLMProcessManager::Config
static ZLMProcessManager::Config createProcessConfig(const ZlmConfig& zlm_config) {
    ZLMProcessManager::Config cfg;
    cfg.debug_terminal = zlm_config.debug_terminal;
    
    // 使用编译时定义的项目根目录，确保在任何位置运行都能找到 tools 目录
#ifdef PROJECT_ROOT_DIR
    std::filesystem::path project_root(PROJECT_ROOT_DIR);
#else
    // 如果没有定义 PROJECT_ROOT_DIR，回退到当前路径的父目录
    std::filesystem::path project_root = std::filesystem::current_path().parent_path();
    LOG_MAIN_WARN_AT("ZLMProcessManager: PROJECT_ROOT_DIR not defined, using parent of current path");
#endif
    
#ifdef _WIN32    
    auto work_dir_path = project_root / "tools" / "win32" / "zlmediakit";
    work_dir_path.make_preferred();  // 规范化路径分隔符
    cfg.work_dir = work_dir_path.string();
    LOG_MAIN_INFO_AT("ZLMProcessManager: Setting work_dir to: {}", cfg.work_dir);
#else
    auto work_dir_path = project_root / "tools" / "linux" / "zlmediakit";
    work_dir_path.make_preferred();  // 规范化路径分隔符
    cfg.work_dir = work_dir_path.string();
    LOG_MAIN_INFO_AT("ZLMProcessManager: Setting work_dir to: {}", cfg.work_dir);
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
}

ZLMProcessManager::~ZLMProcessManager() {    
	LOG_MAIN_INFO_AT("ZLMProcessManager: Destructor called, cleaning up...");
    Stop();
}

std::string ZLMProcessManager::GetZlmediakitPath() {
    std::filesystem::path exec_path;
    
    // 使用编译时定义的项目根目录，确保在任何位置运行都能找到 tools 目录
#ifdef PROJECT_ROOT_DIR
    std::filesystem::path project_root(PROJECT_ROOT_DIR);
#else
    // 如果没有定义 PROJECT_ROOT_DIR，回退到当前路径的父目录
    std::filesystem::path project_root = std::filesystem::current_path().parent_path();
    LOG_MAIN_WARN_AT("ZLMProcessManager: PROJECT_ROOT_DIR not defined, using parent of current path");
#endif
    
#ifdef _WIN32
    exec_path = project_root / "tools" / "win32" / "zlmediakit" / "MediaServer.exe";
#else
    exec_path = project_root / "tools" / "linux" / "zlmediakit" / "MediaServer";
#endif    
    
    // 规范化路径分隔符（Windows 下统一为反斜杠）
    exec_path.make_preferred();
    
    LOG_MAIN_INFO_AT("ZLMProcessManager: Looking for ZLMediaKit at: {}", exec_path.string());
    LOG_MAIN_INFO_AT("ZLMProcessManager: Project root: {}", project_root.string());
    LOG_MAIN_INFO_AT("ZLMProcessManager: Current path: {}", std::filesystem::current_path().string());
    
    if (std::filesystem::exists(exec_path)) {
        LOG_MAIN_INFO_AT("ZLMProcessManager: Found ZLMediaKit at: {}", exec_path.string());
        return exec_path.string();
    }
    LOG_MAIN_ERROR_AT("ZLMProcessManager: ZLMediaKit not found at: {}", exec_path.string());
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

    zlm_process_->AsyncWait([this](const boost::system::error_code& ec, int exit_code) {
		LOG_MAIN_INFO_AT("ZLMProcessManager: Process exit callback triggered with code {}, error: {}", exit_code, ec.message());
        onProcessExit(exit_code, ec);
	});
	LOG_MAIN_INFO_AT("ZLMProcessManager: Started ZLMediaKit process with PID: {}", zlm_process_->Pid());
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
    
    if (zlm_process_.has_value()) {
        boost::system::error_code ec;
        if (zlm_process_->IsRunning(ec)) {
            auto pid = zlm_process_->Pid();
            zlm_process_->RequestExit(ec);
            if (ec) {
                LOG_MAIN_WARN_AT("ZLMProcessManager: Request exit failed for PID {}: {}", pid, ec.message());
            } else {
                LOG_MAIN_INFO_AT("ZLMProcessManager: Requested zlm process exit, PID: {}", pid);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            ec.clear();
            if (zlm_process_->IsRunning(ec)) {
                zlm_process_->Terminate(ec);
                if (ec) {
                    LOG_MAIN_ERROR_AT("ZLMProcessManager: Failed to terminate zlm process PID {}: {}", pid, ec.message());
                } else {
                    LOG_MAIN_INFO_AT("ZLMProcessManager: Terminated zlm process, PID: {}", pid);
                }
            } else if (ec) {
                LOG_MAIN_ERROR_AT("ZLMProcessManager: Failed to query zlm process state: {}", ec.message());
            }

            ec.clear();
            auto exit_code = zlm_process_->Wait(ec);
            if (ec) {
                LOG_MAIN_ERROR_AT("ZLMProcessManager: Exception during wait: {}", ec.message());
            } else {
                LOG_MAIN_INFO_AT("ZLMProcessManager: Process has exited with code: {}", exit_code);
            }
        } else {
            if (ec) {
                LOG_MAIN_ERROR_AT("ZLMProcessManager: Failed to query zlm process state: {}", ec.message());
            } else {
                LOG_MAIN_INFO_AT("ZLMProcessManager: Process was not running, skipping terminate");
            }
        }

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

    process::ProcessOptions options;
    options.executable = zlm_path;
    options.working_directory = config_.work_dir;
    options.terminal_mode = process::TerminalMode::NewTerminal;
    options.window_mode = process::WindowMode::Normal;
    options.terminal_title = std::filesystem::path(zlm_path).filename().string();
    return launchZLMProcess(options, "debug");
}

bool ZLMProcessManager::startZLMProcessNormal() {
    std::string zlm_path = GetZlmediakitPath();
    if (zlm_path.empty()) {
       return false;
    }

    process::ProcessOptions options;
    options.executable = zlm_path;
    options.working_directory = config_.work_dir;
    return launchZLMProcess(options, "normal");
}

bool ZLMProcessManager::launchZLMProcess(const process::ProcessOptions& options, const char* mode) {
    zlm_process_.emplace(ctx_.get_executor());

    boost::system::error_code ec;
    if (!zlm_process_->Start(options, ec)) {
        LOG_MAIN_ERROR_AT("ZLMProcessManager: Failed to start ZLM process in {} mode: {}",
                          mode, zlm_process_->LastError());
        zlm_process_.reset();
        return false;
    }

    return true;
}

void ZLMProcessManager::onProcessExit(int exit_code, const boost::system::error_code& ec) {
    try {
        if (ec == boost::asio::error::operation_aborted) {
			LOG_MAIN_INFO_AT("ZLMProcessManager: Process wait operation was aborted, likely due to Stop() being called");
            return;
        }

        ServiceStatus current_status = status_.load();
        if (current_status == ServiceStatus::STATUS_STOPPING || current_status == ServiceStatus::STATUS_STOPPED) {
			LOG_MAIN_INFO_AT("ZLMProcessManager: Manager is stopping/stopped, skipping exit handler");
            return;
        }

        if (ec) {
            LOG_MAIN_ERROR_AT("ZLM process exited with error: {}", ec.message());
            status_ = ServiceStatus::STATUS_ERROR;
            zlm_process_.reset();
            return;
        }

        if (exit_code != 0) {
			LOG_MAIN_ERROR_AT("ZLM process exited with non-zero code: {}", exit_code);
            status_ = ServiceStatus::STATUS_ERROR;
            zlm_process_.reset();
            return;
        }

        LOG_MAIN_INFO_AT("ZLM process exited normally with code 0");
        status_ = ServiceStatus::STATUS_STOPPED;
        zlm_process_.reset();
    } catch (const std::exception& e) {
		LOG_MAIN_ERROR_AT("ZLMProcessManager: Exception during wait: {}", e.what());
        status_ = ServiceStatus::STATUS_ERROR;
        zlm_process_.reset();
    } catch (...) {
		LOG_MAIN_ERROR_AT("ZLMProcessManager: Unknown exception during wait");
        status_ = ServiceStatus::STATUS_ERROR;
        zlm_process_.reset();
    }
    //TODO: restart ZLMediaKit process
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
