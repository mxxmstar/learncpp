#pragma once

#include "grpc_server.h"
#include "grpc_client.h"
#include "hello.grpc.pb.h"

namespace grpc_module {

/**
 * @brief Hello 服务实现类（服务端）
 */
class HelloServiceImpl final : public simple_grpc::HelloService::Service {
public:
    grpc::Status SayHello(
        grpc::ServerContext* context,
        const simple_grpc::HelloRequest* request,
        simple_grpc::HelloResponse* response) override;
    
    grpc::Status SayHelloStream(
        grpc::ServerContext* context,
        const simple_grpc::HelloRequest* request,
        grpc::ServerWriter<simple_grpc::HelloResponse>* writer) override;
};

/**
 * @brief Hello gRPC 服务器
 */
class HelloGrpcServer : public GrpcServer {
public:
    explicit HelloGrpcServer(const std::string& address = "0.0.0.0:50051");
    
protected:
    void RegisterServices(grpc::ServerBuilder& builder) override;

private:
    HelloServiceImpl service_impl_;
};

/**
 * @brief Hello gRPC 客户端
 */
class HelloGrpcClient : public GrpcClient {
public:
    explicit HelloGrpcClient(const std::string& target = "localhost:50051");
    
    /**
     * @brief Unary RPC - 简单请求响应
     */
    bool SayHello(const std::string& name, 
                  simple_grpc::HelloResponse* response,
                  int timeout_ms = 5000);
    
    /**
     * @brief Server Streaming RPC - 服务端流式响应
     */
    bool SayHelloStream(const std::string& name,
                        int count,
                        std::function<void(const simple_grpc::HelloResponse&)> callback,
                        int timeout_ms = 10000);

private:
    std::unique_ptr<simple_grpc::HelloService::Stub> stub_;
    bool CreateStub();
};

} // namespace grpc_module
