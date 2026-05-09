#include "alg/inference/prepost_processor.h"
#include "common/log/logmanager.h"
#include <iostream>

PrePostProcessor::PrePostProcessor() {
}

PrePostProcessor::~PrePostProcessor() {
}

std::shared_ptr<ov::Model> PrePostProcessor::Configure(std::shared_ptr<ov::Model> model, const PreProcessConfig& config) {
    try {
        config_ = config;
        
        LOG_MAIN_INFO_AT("Configuring PrePostProcessor:");
        LOG_MAIN_INFO_AT("  Input format: {}", 
            config.input_format == ImageFormat::RGB ? "RGB" :
            config.input_format == ImageFormat::BGR ? "BGR" :
            config.input_format == ImageFormat::NV12 ? "NV12" :
            config.input_format == ImageFormat::NV21 ? "NV21" :
            config.input_format == ImageFormat::YUV420P ? "YUV420P" : "GRAY");
        LOG_MAIN_INFO_AT("  Target size: {}x{}", config.target_size.second, config.target_size.first);
        LOG_MAIN_INFO_AT("  Normalize: {}", config.normalize ? "yes" : "no");
        LOG_MAIN_INFO_AT("  Output layout: {}", config.output_layout);
        LOG_MAIN_INFO_AT("  Output type: {}", config.output_type);
        
        // 获取模型的输入信息
        auto input_info = model->inputs();
        if (input_info.size() == 0) {
            LOG_MAIN_ERROR_AT("Model has no inputs");
            return nullptr;
        }
        
        auto input = input_info[0];
        auto element_type = input.get_element_type();
        auto shape = input.get_shape();
        
        LOG_MAIN_INFO_AT("Model input: shape=[{},{},{},{}], type={}",
            shape[0], shape[1], shape[2], shape[3], element_type.get_type_name());
        
        // 创建 PrePostProcessor
        ov::preprocess::PrePostProcessor ppp(model);
        
        // 1. 设置输入预处理
        SetupColorConversion(ppp);
        SetupResize(ppp);
        SetupNormalization(ppp);
        SetupLayoutAndType(ppp);
        
        // 2. 构建模型（应用预处理）
        auto processed_model = ppp.build();
        
        configured_ = true;
        LOG_MAIN_INFO_AT("PrePostProcessor configured successfully");
        
        return processed_model;
        
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("Failed to configure PrePostProcessor: {}", e.what());
        configured_ = false;
        return nullptr;
    }
}

ov::InferRequest PrePostProcessor::CreateInferRequest() {
    if (!configured_) {
        throw std::runtime_error("PrePostProcessor not configured");
    }
    
    // 注意：这里需要从已配置的模型创建推理请求
    // 由于我们修改了 model，需要在外部保存引用
    // 这个函数暂时返回空，实际使用时需要传入 model
    throw std::runtime_error("Use Configure with model reference instead");
}

void PrePostProcessor::SetupColorConversion(ov::preprocess::PrePostProcessor& ppp) {
    auto& input_info = ppp.input();
    
    // 根据输入格式设置颜色转换
    switch (config_.input_format) {
        case ImageFormat::RGB:
            // RGB 不需要转换
            input_info.tensor().set_color_format(ov::preprocess::ColorFormat::RGB);
            break;
            
        case ImageFormat::BGR:
            // BGR -> RGB（如果模型期望 RGB）
            input_info.tensor().set_color_format(ov::preprocess::ColorFormat::BGR);
            input_info.preprocess().convert_color(ov::preprocess::ColorFormat::RGB);
            break;
            
        case ImageFormat::NV12:
            // NV12 -> RGB
            input_info.tensor().set_color_format(ov::preprocess::ColorFormat::NV12_SINGLE_PLANE);
            input_info.preprocess().convert_color(ov::preprocess::ColorFormat::RGB);
            break;
            
        case ImageFormat::NV21:
            // NV21 -> RGB (OpenVINO 不直接支持 NV21，需要手动转换或使用 NV12)
            // 这里暂时使用 NV12，实际应用中可能需要额外的处理
            LOG_MAIN_WARN_AT("NV21 format not directly supported, using NV12 as fallback");
            input_info.tensor().set_color_format(ov::preprocess::ColorFormat::NV12_SINGLE_PLANE);
            input_info.preprocess().convert_color(ov::preprocess::ColorFormat::RGB);
            break;
            
        case ImageFormat::YUV420P:
            // YUV420P (I420) -> RGB
            input_info.tensor().set_color_format(ov::preprocess::ColorFormat::I420_SINGLE_PLANE);
            input_info.preprocess().convert_color(ov::preprocess::ColorFormat::RGB);
            break;
            
        case ImageFormat::GRAY:
            // 灰度不需要颜色转换
            input_info.tensor().set_color_format(ov::preprocess::ColorFormat::GRAY);
            break;
            
        default:
            LOG_MAIN_WARN_AT("Unknown input format, skipping color conversion");
            break;
    }
}

void PrePostProcessor::SetupResize(ov::preprocess::PrePostProcessor& ppp) {
    auto& input_info = ppp.input();
    
    // 设置目标尺寸
    int target_h = config_.target_size.first;
    int target_w = config_.target_size.second;
    
    // 使用 linear 插值进行缩放（等同于 bilinear）
    input_info.preprocess().resize(ov::preprocess::ResizeAlgorithm::RESIZE_LINEAR);
    
    // 注意：OpenVINO PrePostProcessor 的 resize 会自动适配模型输入形状
    // 不需要手动设置目标尺寸，它会从模型输入形状中推断
}

void PrePostProcessor::SetupNormalization(ov::preprocess::PrePostProcessor& ppp) {
    if (!config_.normalize) {
        return;
    }
    
    auto& input_info = ppp.input();
    
    // 设置归一化参数
    // mean 和 std 应该是针对每个通道的
    if (config_.mean.size() >= 3 && config_.std.size() >= 3) {
        // 对于 RGB 图像，分别设置每个通道的均值和标准差
        input_info.preprocess().mean({config_.mean[0], config_.mean[1], config_.mean[2]});
        input_info.preprocess().scale({config_.std[0], config_.std[1], config_.std[2]});
    } else {
        // 使用默认值
        input_info.preprocess().mean({0.0f, 0.0f, 0.0f});
        input_info.preprocess().scale({1.0f, 1.0f, 1.0f});
    }
}

void PrePostProcessor::SetupLayoutAndType(ov::preprocess::PrePostProcessor& ppp) {
    auto& input_info = ppp.input();
    auto& output_info = ppp.output();
    
    // 设置输出布局
    if (config_.output_layout == "NCHW") {
        output_info.tensor().set_layout("NCHW");
    } else if (config_.output_layout == "NHWC") {
        output_info.tensor().set_layout("NHWC");
    }
    
    // 设置输出数据类型
    if (config_.output_type == "f32") {
        output_info.tensor().set_element_type(ov::element::f32);
    } else if (config_.output_type == "u8") {
        output_info.tensor().set_element_type(ov::element::u8);
    }
}
