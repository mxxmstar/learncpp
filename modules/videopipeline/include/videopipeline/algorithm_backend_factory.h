#pragma once

#include "videopipeline/i_algorithm_backend.h"
#include "videopipeline/backends/null_backend.h"
#include "videopipeline/backends/openvino_backend.h"  // OpenVINO 后端
#include <memory>
#include <string>

/// @brief 算法后端工厂
/// 
/// 根据配置创建对应的算法后端实例
class AlgorithmBackendFactory {
public:
    /// @brief 创建算法后端
    /// @param config 算法配置
    /// @return 算法后端实例，失败返回 nullptr
    static std::unique_ptr<IAlgorithmBackend> create(const AlgorithmConfig& config) {
        std::string algo_type = config.getActiveAlgorithm();
        
        if (algo_type == "openvino") {
            // ✅ Phase 2: OpenVINO 后端已实现
            return std::make_unique<OpenVINOBackend>();
            
        } else if (algo_type == "opencv") {
            // TODO: Phase 3 实现
            // return std::make_unique<OpenCVBackend>();
            LOG_MAIN_WARN_AT("OpenCV backend not yet implemented, using NullBackend");
            return std::make_unique<NullBackend>();
            
        } else if (algo_type == "grpc") {
            // TODO: Phase 4 实现
            // return std::make_unique<GrpcBackend>();
            LOG_MAIN_WARN_AT("gRPC backend not yet optimized, using NullBackend");
            return std::make_unique<NullBackend>();
            
        } else {
            // 默认使用空后端
            LOG_MAIN_INFO_AT("No algorithm configured, using NullBackend");
            return std::make_unique<NullBackend>();
        }
    }
    
    /// @brief 获取支持的算法类型列表
    static std::vector<std::string> getSupportedTypes() {
        return {"none", "openvino", "opencv", "grpc"};
    }
};
