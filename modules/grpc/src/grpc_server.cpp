#include "grpc/grpc_server.h"
#include <iostream>

namespace grpc_module {

GrpcServer::GrpcServer(const std::string& address)
    : address_(address) {
}

GrpcServer::~GrpcServer() {
    Stop();
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
}

bool GrpcServer::Initialize() {
    grpc::ServerBuilder builder;
    
    // 监听指定地址（使用不安全连接，用于测试）
    builder.AddListeningPort(address_, grpc::InsecureServerCredentials());
    
    // 注册服务
    RegisterServices(builder);
    
    // 构建并启动服务器
    server_ = builder.BuildAndStart();
    if (!server_) {
        std::cerr << "[GrpcServer] Failed to start server on " << address_ << std::endl;
        return false;
    }
    
    std::cout << "[GrpcServer] Server listening on " << address_ << std::endl;
    return true;
}

bool GrpcServer::Start() {
    if (running_) {
        std::cerr << "[GrpcServer] Server is already running" << std::endl;
        return false;
    }
    
    if (!Initialize()) {
        return false;
    }
    
    // 在后台线程中运行服务器
    running_ = true;
    server_thread_ = std::thread(&GrpcServer::RunServer, this);
    
    return true;
}

void GrpcServer::Stop() {
    if (!running_) {
        return;
    }
    
    std::cout << "[GrpcServer] Stopping server..." << std::endl;
    running_ = false;
    
    if (server_) {
        server_->Shutdown();
    }
}

void GrpcServer::Wait() {
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
    std::cout << "[GrpcServer] Server stopped" << std::endl;
}

void GrpcServer::RunServer() {
    if (server_) {
        server_->Wait();
    }
}

} // namespace grpc_module
