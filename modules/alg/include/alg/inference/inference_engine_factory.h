#pragma once

#include "alg/inference/i_inference_engine.h"
#include <memory>
#include <string>
#include <functional>
#include <map>

/// @brief 推理引擎工厂
class InferenceEngineFactory {
public:
    /// @brief 创建推理引擎
    /// @param type 引擎类型字符串 ("openvino_cpu", "tensorrt", etc.)
    /// @param config 配置
    static std::unique_ptr<IInferenceEngine> Create(
        const std::string& type, 
        const InferenceConfig& config);
    
    /// @brief 注册自定义引擎创建器
    using CreatorFunc = std::function<std::unique_ptr<IInferenceEngine>(const InferenceConfig&)>;
    static void RegisterCreator(const std::string& name, CreatorFunc creator);
    
private:
    static std::map<std::string, CreatorFunc>& GetCreators();
};
