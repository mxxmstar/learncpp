#include "zlmediakit/zlm_hookserver.h"
#include "zlmediakit/zlm_manager.h"
#include <boost/process/windows/creation_flags.hpp>
#include <boost/process/windows/show_window.hpp>
#include <boost/process.hpp>
#include <boost/asio/steady_timer.hpp>
#include <iostream>
#include "zlmediakit/zlm_proxy_pull_manager.h"
#include "log/logmanager.h"
#include "net/httpclientpool.h"
#include "net/httpserver.h"
#include "net/httprouter.h"
using namespace Net;

void testZlm() {
    std::string zlm_path = ZLMProcessManager::GetZlmediakitPath();
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
    
    // 创建一个 work guard 来保持 io_context 运行
    auto work_guard = boost::asio::make_work_guard(ctx);
    
    ZLMProcessManager::Config cfg;
    cfg.debug_terminal = false;
    
    // 使用 shared_ptr 管理生命周期
    auto zlm_mgr = std::make_shared<ZLMProcessManager>(ctx, cfg);
    
    if (zlm_mgr->Start()) {
        std::cout << "ZLM Process started successfully." << std::endl;
        std::cout << "Program will auto-stop after 5 seconds..." << std::endl;
        
        // 设置一个定时器，5 秒后自动停止
        boost::asio::steady_timer timer(ctx, std::chrono::seconds(5));
        timer.async_wait([&](const boost::system::error_code& ec) {
            if (!ec) {
                std::cout << "\n=== Auto-stopping after 5 seconds ===" << std::endl;
                ctx.stop();
            }
        });
        
        // 运行 io_context，但允许通过 stop() 停止
        try {
            ctx.run();
        } catch (const std::exception& e) {
            std::cerr << "Exception during io_context.run(): " << e.what() << std::endl;
        }
        
        std::cout << "io_context stopped, cleaning up ZLM manager..." << std::endl;
    }
    else {
        std::cout << "Failed to start ZLM Process." << std::endl;
    }
    
    // 显式释放 shared_ptr，触发析构函数
    zlm_mgr.reset();
    
    std::cout << "ZLM Manager cleaned up" << std::endl;
    std::cout << "testZlmManager() function exiting..." << std::endl;
}

void testZlmApiClient() {
    std::cout << "\n=== Test ZLM API Client ===" << std::endl;
    
    boost::asio::io_context ctx;
    ZLMAddressConfig cfg;
    cfg.host = "127.0.0.1";
    cfg.port = 80;
    cfg.secret = "sphrh7r2VafHUILiTVyK3rm1C6hnUYpZ";  // 替换为你的密钥
    
    // 创建连接池
    HttpClientPool pool;
    HttpClientPool::Config config;
    config.host = "127.0.0.1";
    config.port = 80;
    config.init_size = 2;
    config.max_size = 5;
    pool.Init(ctx, config);
    
    // 创建 ZLMApiClient（需要传入 pool）
    auto client = ZLMApiClient(ctx, &pool, cfg);

    // 测试 1：获取媒体列表（无参数）
    std::cout << "\n--- Test 1: Get Media List (no params) ---" << std::endl;
    client.ProxyPull().GetMediaList();
    
    //// 测试 2：获取指定应用的媒体列表
    //std::cout << "\n--- Test 2: Get Media List for specific app ---" << std::endl;
    //boost::json::object params;
    //params["__schema"] = "rtmp";
    //params["__host"] = "__defaultVhost__";
    //params["app"] = "live";
    //client.ProxyPull().GetMediaList(params);
    
    //// 测试 3：添加拉流代理
    //std::cout << "\n--- Test 3: Add Stream Proxy ---" << std::endl;
    //ZLMStreamProxyInfo proxy_info;
    //proxy_info.vhost = "__defaultVhost__";
    //proxy_info.app = "live";
    //proxy_info.stream = "test_stream";
    //proxy_info.url = "rtmp://example.com/live/test";
    //client.ProxyPull().AddStreamProxy(proxy_info);
    //
    //// 测试 4：查询拉流代理信息
    //std::cout << "\n--- Test 4: Get Proxy Info ---" << std::endl;
    //client.ProxyPull().GetProxyInfo(proxy_info);
    //
    //// 测试 5：删除拉流代理
    //std::cout << "\n--- Test 5: Delete Stream Proxy ---" << std::endl;
    //client.ProxyPull().DelStreamProxy(proxy_info);
    
    // 运行 io_context 处理异步请求
    std::cout << "\nRunning io_context..." << std::endl;
    ctx.run();
    
    std::cout << "\n=== All ZLM tests completed! ===" << std::endl;
}

int main() {
    LogManager& log_manager = LogManager::getInstance();
    log_manager.Init();

    std::cout << "Initializing ZLMediaKit Hook Server Test..." << std::endl;
    
    try {        
        //// 创建 Hook 处理器，使用默认密钥
        //ZLMHookHandler hook_handler("test_secret");
        //hook_handler.RegisterRoutes();

        //// 创建主io_context
        //boost::asio::io_context main_io_context;
        //
        //// 创建工作池
        //auto& worker_pool = AsioIOContextPool::GetInstance(AsioIOContextPool::ServiceType::HTTP);
        //
        //// 创建HTTP服务器，监听端口8080
        //AsioHttpServer server(main_io_context, worker_pool, 8080);
        //
        //// 启动服务器
        //server.Start();        
        // 运行主io_context
        //main_io_context.run();                
        
        //testZlmManager();
        testZlmApiClient();
        std::cout << "ZLMediaKit Hook Server is running!" << std::endl;

        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        LOG_MAIN_ERROR_AT("ZLM Hook Server test failed: {}", e.what());
        return 1;
    }
    
    std::cout << "ZLMediaKit Hook Server test completed." << std::endl;
    return 0;
}
