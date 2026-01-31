#include <boost/process/windows/creation_flags.hpp>
#include <boost/process/windows//show_window.hpp>
#include <filesystem>
#include <vector>
#include <iostream>

#include "zlmediakit/zlm_manager.h"
#include "log/logmanager.h"
#include "net/httprouter.h"


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

	// asio 驱动的进程退出回调
    zlm_process_->async_wait([this](const std::error_code& ec, int exit_code) {
        onProcessExit(exit_code, ec);
	});
    std::cout << "ZLM process started" << std::endl;
    return true;
}

void ZLMProcessManager::Stop() {
    if (status_ == ServiceStatus::STATUS_STOPPED) {
        return;
    }

    status_ = ServiceStatus::STATUS_STOPPING;
    

    // 终止 ZLMediaKit 进程
    if (zlm_process_.has_value()) {
        zlm_process_->terminate();
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
		boost::process::windows::show_window_maximized
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
    //std::string zlm_path = GetZlmediakitPath();
    //if (zlm_path.empty()) {
    //    return false;
    //}

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
    if (ec) {
		LOG_MAIN_ERROR_AT("ZLM process exited with error: {}", ec.message());
        return;
    }

	// running 状态下非正常退出，标记为错误状态
	if (status_ == ServiceStatus::STATUS_RUNNING) {
        status_ = ServiceStatus::STATUS_ERROR;
        LOG_MAIN_ERROR_AT("ZLM process exited unexpectedly with code: {}", exit_code);

        std::cout << "ZLM process exited unexpectedly with code: " << exit_code << std::endl;
        //TODO: 重启 ZLMediaKit 进程
    }
}

//bool ZLMManager::Start()
//{
//	std::string zlm_key = "zlmediakit"; // 假设这是从配置文件或环境变量中获取的
//	// 注册安全路由 /hook
//    HttpRouter::GetInstance().RegisterSecureRoute("/zlmediakit/hook",
//        zlm_key,
//        [this](const boost::json::object& req, boost::json::object& rsp) {
//            try {
//				// TODO: 处理 ZLMediaKit 的 Hook 请求
//            } catch (std::exception& e) {
//                LOG_MAIN_ERROR_AT("exception: {}", e.what());
//                rsp["code"] = -1;   // TODO:错误码
//                rsp["message"] = e.what();
//            }
//        }
//	);
//
//	// 注册普通路由
//    HttpRouter::GetInstance().RegisterRoute("/zlmediakit/health",
//        [](const boost::json::object& req, boost::json::object& rsp) {
//			// 简单的健康检查响应
//            rsp["code"] = 0;
//            rsp["message"] = "ZLMediaKit is running";
//        }
//	);
//
//    // 启动 ZLMediaKit 进程
//	process_.Start();
//}
