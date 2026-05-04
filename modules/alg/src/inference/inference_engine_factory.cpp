#include "alg/inference/inference_engine_factory.h"
#include "alg/inference/openvino_cpu_engine.h"
#include "common/log/logmanager.h"

std::unique_ptr<IInferenceEngine> InferenceEngineFactory::Create(
    const std::string& type, 
    const InferenceConfig& config) {
    
    auto& creators = GetCreators();
    
    auto it = creators.find(type);
    if (it != creators.end()) {
        LOG_MAIN_INFO_AT("Creating inference engine: {}", type);
        return it->second(config);
    }
    
    LOG_MAIN_ERROR_AT("Unknown inference engine type: {}", type);
    return nullptr;
}

void InferenceEngineFactory::RegisterCreator(
    const std::string& name, 
    CreatorFunc creator) {
    auto& creators = GetCreators();
    creators[name] = std::move(creator);
    LOG_MAIN_INFO_AT("Registered inference engine creator: {}", name);
}

std::map<std::string, InferenceEngineFactory::CreatorFunc>& 
InferenceEngineFactory::GetCreators() {
    static std::map<std::string, CreatorFunc> creators;
    
    // 懒加载：首次调用时注册默认引擎
    if (creators.empty()) {
        // OpenVINO CPU
        creators["openvino_cpu"] = [](const InferenceConfig& config) {
            auto engine = std::make_unique<OpenVinoCpuEngine>();
            if (!engine->LoadModel(config)) {
                LOG_MAIN_ERROR_AT("Failed to load OpenVINO CPU engine");
                return std::unique_ptr<IInferenceEngine>(nullptr);
            }
            return std::unique_ptr<IInferenceEngine>(std::move(engine));
        };
        
        // OpenVINO GPU
        creators["openvino_gpu"] = [](const InferenceConfig& config) {
            LOG_MAIN_WARN_AT("OpenVINO GPU engine not implemented yet");
            return std::unique_ptr<IInferenceEngine>(nullptr);
        };
        
        // TensorRT
        creators["tensorrt"] = [](const InferenceConfig& config) {
            LOG_MAIN_WARN_AT("TensorRT engine not implemented yet");
            return std::unique_ptr<IInferenceEngine>(nullptr);
        };
        
        // ONNX Runtime CPU
        creators["onnxruntime_cpu"] = [](const InferenceConfig& config) {
            LOG_MAIN_WARN_AT("ONNX Runtime CPU engine not implemented yet");
            return std::unique_ptr<IInferenceEngine>(nullptr);
        };
        
        // ONNX Runtime CUDA
        creators["onnxruntime_cuda"] = [](const InferenceConfig& config) {
            LOG_MAIN_WARN_AT("ONNX Runtime CUDA engine not implemented yet");
            return std::unique_ptr<IInferenceEngine>(nullptr);
        };
    }
    
    return creators;
}
