#include <iostream>
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "log/logger.h"
int main() {
    std::cout << "Hello, cross-platform C++!" << std::endl;
#ifdef _WIN32
    std::cout << "Building on Windows" << std::endl;
#elif __linux__
    std::cout << "Building on Linux" << std::endl;
#endif
    auto console = spdlog::stdout_color_mt("console");
    auto err_logger = spdlog::stderr_color_mt("stderr");
    spdlog::get("console")->info("Welcome to spdlog!");

    // 初始化日志系统
    LogManager::getInstance().Init();
    
    // // 使用不同协议的logger进行日志记录
    // LOG_MAIN("流媒体服务器启动");
    // LOG_HTTP("HTTP服务器已启动，监听端口: 8080");
    // LOG_RTSP("RTSP服务器已启动，监听端口: 554");
    // LOG_RTMP("RTMP服务器已启动，监听端口: 1935");
    // LOG_NETWORK("网络层初始化完成");
    // LOG_STREAM("流处理模块已加载");
    
    // // 不同级别的日志示例
    // LOG_HTTP_DEBUG("处理HTTP请求: GET /stream");
    // LOG_RTSP_DEBUG("收到RTSP DESCRIBE请求");
    // LOG_RTMP_DEBUG("建立RTMP连接: client_id=12345");
    
    // LOG_HTTP("HTTP响应: 200 OK");
    // LOG_RTSP("RTSP会话已创建: session_id=abc123");
    // LOG_RTMP("RTMP流已发布: stream_key=live_123456");
    
    // LOG_NETWORK_DEBUG("客户端连接: IP=192.168.1.100, port=54321");
    // LOG_STREAM_DEBUG("流媒体数据传输中: bitrate=2Mbps");
    
    // // 模拟错误情况
    // LOG_RTSP_ERROR("RTSP错误: 无法解析SDP描述");
    // LOG_NETWORK_ERROR("网络错误: 客户端连接超时");
    
    std::cout << "日志示例程序执行完成。请检查 logs 目录下的日志文件。" << std::endl;

    return 0;
}