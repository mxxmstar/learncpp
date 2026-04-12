#pragma once

#include <string>
#include <memory>
#include <atomic>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>

namespace grpc_module {

/**
 * @brief gRPC 服务端基类封装
 * 
 * 提供 gRPC 服务端的统一生命周期管理：
 * - Initialize: 初始化服务器
 * - Start: 启动服务器（后台线程）
 * - Stop: 停止服务器
 * - Wait: 等待服务器退出
 */
class GrpcServer {
public:
    /**
     * @brief 构造函数
     * @param address 监听地址，例如 "0.0.0.0:50051"
     */
    explicit GrpcServer(const std::string& address);
    virtual ~GrpcServer();

    // 禁止拷贝
    GrpcServer(const GrpcServer&) = delete;
    GrpcServer& operator=(const GrpcServer&) = delete;

    /**
     * @brief 初始化服务器
     * @return 成功返回 true
     */
    virtual bool Initialize();

    /**
     * @brief 启动服务器（在后台线程中运行）
     * @return 成功返回 true
     */
    bool Start();

    /**
     * @brief 停止服务器
     */
    void Stop();

    /**
     * @brief 等待服务器退出
     */
    void Wait();

    /**
     * @brief 是否正在运行
     */
    bool IsRunning() const { return running_; }

    /**
     * @brief 获取服务器地址
     */
    const std::string& GetAddress() const { return address_; }

protected:
    /**
     * @brief 注册服务到 ServerBuilder
     * 子类需要实现此方法，调用 builder.RegisterService()
     */
    virtual void RegisterServices(grpc::ServerBuilder& builder) = 0;

private:
    void RunServer();

    std::string address_;
    std::unique_ptr<grpc::Server> server_;
    std::thread server_thread_;
    std::atomic<bool> running_{false};
};

} // namespace grpc_module
