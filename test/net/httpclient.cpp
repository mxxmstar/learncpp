#include "net/http_client/http_client.h"
#include "net/io_context_pool/asio_io_context_pool.h"
#include "log/logmanager.h"
#include <boost/json.hpp>
#include <iostream>
using namespace Net;
int main() {
    LogManager& log_manager = LogManager::getInstance();
    log_manager.Init();
    std::cout << "LogManager initialized" << std::endl;
    
    // 创建主io_context
    boost::asio::io_context main_io_context;
    
    // 创建工作池
    auto& worker_pool = AsioIOContextPool::GetInstance();

    boost::json::object rsp_obj;
    // 创建请求对象
    boost::json::object req_obj;    
    req_obj["test_data"] = "hello world";
    req_obj["number"] = 123;    
    //AsioSyncHttpClient client("httpbin.org", 80);
    //
    //// 发送POST请求到 /post 路径

    //bool success = client.PostJson("/post", req_obj, rsp_obj);  // 在这里指定路径

    //AsioAsyncHttpClient client(main_io_context, "httpbin.org", 80);// 需要通过make_shared创建
    auto client = std::make_shared<AsioAsyncHttpClient>(main_io_context, "httpbin.org", 80);
    
    bool success = 1;
    client->PostJson("/post", req_obj, [](bool success, const boost::json::object& rsp_obj) {
		std::cout << "Response received successfully!" << std::endl;
        for (auto& [key, value] : rsp_obj) {
            std::cout << key << ": " << value << std::endl;
        }
        });
    
    if (success) {
        // 打印响应
        std::cout << "Response received successfully!" << std::endl;
        for (auto& [key, value] : rsp_obj) {
            std::cout << key << ": " << value << std::endl;
        }
    } else {
        std::cout << "Failed to receive response" << std::endl;
    }
    main_io_context.run();
    return 0;
}
