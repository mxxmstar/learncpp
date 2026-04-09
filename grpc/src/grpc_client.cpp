#include "grpc_client.h"
#include <iostream>
#include <chrono>

namespace grpc_module {

GrpcClient::GrpcClient(const std::string& target)
    : target_(target) {
    // 配置 Channel Arguments
    grpc::ChannelArguments args;
    args.SetMaxReceiveMessageSize(-1);  // 无限制
    
    // 创建 Channel（使用不安全连接）
    channel_ = grpc::CreateCustomChannel(target_, grpc::InsecureChannelCredentials(), args);
}

bool GrpcClient::WaitForConnected(int timeout_seconds) {
    auto deadline = std::chrono::system_clock::now() + 
                    std::chrono::seconds(timeout_seconds);
    
    return channel_->WaitForConnected(deadline);
}

grpc_connectivity_state GrpcClient::GetState(bool try_to_connect) {
    return channel_->GetState(try_to_connect);
}

std::unique_ptr<grpc::ClientContext> GrpcClient::CreateContext(int timeout_ms) {
    auto context = std::make_unique<grpc::ClientContext>();
    
    if (timeout_ms > 0) {
        auto deadline = std::chrono::system_clock::now() + 
                        std::chrono::milliseconds(timeout_ms);
        context->set_deadline(deadline);
    }
    
    return context;
}

} // namespace grpc_module
