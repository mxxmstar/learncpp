#include "application/application.h"
// #include "video_pipeline/video_pipeline.h"  // 暂时注释，videopipeline 模块未启用
#include "log/logmanager.h"
#include <iostream>
#include <thread>
#include <boost/asio.hpp>

int main() {
    try {
         // 获取 Application 实例
        auto& app = Application::getInstance();
        
         // ==================== 1. 加载配置 ====================
        app.loadConfig("../tools/config.yaml");

        LogManager& log_manager = LogManager::getInstance();
        log_manager.Init();
        
        
        // // ==================== 2. 初始化日志系统 ====================
        // if (!app.initLogger("logs", "debug")) {
        //     std::cerr << "Failed to initialize logger" << std::endl;
        //     return 1;
        // }
        
        // // ==================== 3. 注册服务（依赖注入）====================
        
        // // 创建 io_context
        // auto io_ctx = std::make_shared<boost::asio::io_context>();
        // app.registerService<boost::asio::io_context>("io_context", *io_ctx);
        
        // // 创建视频流水线配置
        // PipelineConfig pipeline_config;
        // pipeline_config.channel_id = 1;
        // pipeline_config.stream_url = "http://127.0.0.1/live/proxy_cam1.live.flv";
        // pipeline_config.reconnect_delay = 3;
        // pipeline_config.max_reconnect_attempts = -1;
        // pipeline_config.decoder_threads = 2;
        
        // // 注册视频流水线服务
        // app.registerService<VideoPipeline>("video_pipeline", *io_ctx, pipeline_config);
        
        // // ==================== 4. 注册生命周期回调 ====================
        
        // // 初始化阶段
        // app.onInit([&app]() {
        //     std::cout << "[Main] Initializing video pipeline..." << std::endl;
            
        //     auto pipeline = app.getService<VideoPipeline>("video_pipeline");
        //     if (!pipeline) {
        //         std::cerr << "[Main] Failed to get video pipeline service" << std::endl;
        //         return false;
        //     }
            
        //     // 设置帧处理回调
        //     int frame_count = 0;
        //     pipeline->setFrameOutputCallback(
        //         [&frame_count](int channel_id, cv::Mat&& frame, int64_t pts) {
        //             frame_count++;
                    
        //             if (frame_count % 30 == 0) {
        //                 std::cout << "[Main] Processed " << frame_count << " frames" << std::endl;
        //             }
        //         }
        //     );
            
        //     return true;
        // });
        
        // // 启动阶段
        // app.onStart([&app]() {
        //     std::cout << "[Main] Starting video pipeline..." << std::endl;
            
        //     auto pipeline = app.getService<VideoPipeline>("video_pipeline");
        //     if (!pipeline) {
        //         std::cerr << "[Main] Failed to get video pipeline service" << std::endl;
        //         return false;
        //     }
            
        //     if (!pipeline->start()) {
        //         std::cerr << "[Main] Failed to start video pipeline" << std::endl;
        //         return false;
        //     }
            
        //     std::cout << "[Main] Video pipeline started successfully" << std::endl;
        //     return true;
        // });
        
        // // 停止阶段
        // app.onStop([&app]() {
        //     std::cout << "[Main] Stopping video pipeline..." << std::endl;
            
        //     auto pipeline = app.getService<VideoPipeline>("video_pipeline");
        //     if (pipeline) {
        //         pipeline->stop();
        //     }
            
        //     auto io_ctx = app.getService<boost::asio::io_context>("io_context");
        //     if (io_ctx) {
        //         io_ctx->stop();
        //     }
            
        //     std::cout << "[Main] All services stopped" << std::endl;
        // });
        
        // // ==================== 5. 运行应用程序 ====================
        // std::cout << "\n[Main] Starting application loop..." << std::endl;
        
        // // 在后台线程运行 io_context
        // std::thread io_thread([io_ctx]() {
        //     std::cout << "[IO Thread] Running io_context..." << std::endl;
        //     io_ctx->run();
        //     std::cout << "[IO Thread] io_context stopped" << std::endl;
        // });
        
        // // 运行主循环（阻塞直到收到停止信号）
        // int exit_code = app.run();
        
        // // 等待 io 线程结束
        // if (io_thread.joinable()) {
        //     io_thread.join();
        // }
        
        // return exit_code;
    }
    catch (const std::exception& e) {
        std::cerr << "\n[Main] Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
