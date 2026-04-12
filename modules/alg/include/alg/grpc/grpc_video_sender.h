#pragma once

#include "alg/grpc/video_grpc_client.h"
#include <atomic>
#include <memory>
#include <string>

/**
 * @brief gRPC 视频发送器（第一阶段简化版）
 * 
 * 功能：
 * - 使用现有的 VideoGrpcClient
 * - 实现基本的双向流通信
 * - 简单的帧发送（无帧率控制）
 */
class GrpcVideoSender {
public:
    /**
     * @brief 构造函数
     * @param server_address gRPC 服务器地址
     * @param target_fps 目标帧率（0 表示不限制）
     */
    explicit GrpcVideoSender(const std::string& server_address, int target_fps = 10);
    
    /// @brief 析构函数
    ~GrpcVideoSender();
    
    // 禁止拷贝
    GrpcVideoSender(const GrpcVideoSender&) = delete;
    GrpcVideoSender& operator=(const GrpcVideoSender&) = delete;
    
    /**
     * @brief 启动 gRPC 连接和检测流
     * @return true 成功，false 失败
     */
    bool start();
    
    /**
     * @brief 停止 gRPC 连接
     */
    void stop();
    
    /**
     * @brief 发送视频帧
     * @param jpeg_data JPEG 编码的数据
     * @param width 宽度
     * @param height 高度
     * @param frame_id 帧 ID
     * @param timestamp 时间戳
     * @return true 成功，false 失败
     */
    bool sendFrame(const std::vector<uint8_t>& jpeg_data, 
                   int width, 
                   int height,
                   const std::string& frame_id,
                   int64_t timestamp);
    
    /**
     * @brief 是否已连接
     */
    bool isConnected() const;
    
private:
    /// @brief gRPC 客户端
    std::unique_ptr<grpc_module::VideoGrpcClient> grpc_client_;
    
    /// @brief 服务器地址
    std::string server_address_;
    
    /// @brief 运行状态
    std::atomic<bool> running_{false};
};
