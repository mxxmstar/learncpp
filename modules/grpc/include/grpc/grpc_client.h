#pragma once

#include <string>
#include <memory>
#include <grpcpp/grpcpp.h>

namespace grpc_module {

/**
 * @brief gRPC 客户端基类封装
 * 
 * 提供 gRPC 客户端的基础功能：
 * - 连接管理
 * - Channel 管理
 * - 超时设置
 */
class GrpcClient {
public:
    /**
     * @brief 构造函数
     * @param target 服务器地址，例如 "localhost:50051"
     */
    explicit GrpcClient(const std::string& target);
    virtual ~GrpcClient() = default;

    // 禁止拷贝
    GrpcClient(const GrpcClient&) = delete;
    GrpcClient& operator=(const GrpcClient&) = delete;

    /**
     * @brief 获取 Channel
     */
    std::shared_ptr<grpc::Channel> GetChannel() const { return channel_; }

    /**
     * @brief 获取目标地址
     */
    const std::string& GetTarget() const { return target_; }

    /**
     * @brief 等待连接就绪
     * @param timeout_seconds 超时时间（秒）
     * @return 成功返回 true
     */
    bool WaitForConnected(int timeout_seconds = 5);

    /**
     * @brief 获取当前连接状态
     */
    grpc_connectivity_state GetState(bool try_to_connect = false);

protected:
    /**
     * @brief 创建带超时的 ClientContext
     * @param timeout_ms 超时时间（毫秒）
     */
    static std::unique_ptr<grpc::ClientContext> CreateContext(int timeout_ms = 5000);

    std::string target_;
    std::shared_ptr<grpc::Channel> channel_;
};

} // namespace grpc_module
