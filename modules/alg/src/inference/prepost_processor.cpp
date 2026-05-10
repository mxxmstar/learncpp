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
        LOG_MAIN_INFO_AT("  model layout: {}", config.layout);
        LOG_MAIN_INFO_AT("  model type: {}", config.dtype);
        
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
        
        // 1. 设置输入 Tensor 信息
        SetupInputTensor(ppp);

        // 2. 设置模型输入 Layout
        SetupModelLayout(ppp);

        // 3. 设置颜色转换
        SetupColorConversion(ppp);

        // 4. 设置数据类型转换
        SetupDataType(ppp);

        // 5. Resize
        SetupResize(ppp);

        // 6. Normalize
        SetupNormalization(ppp);
        
        // 7. 构建模型（应用预处理）
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

void PrePostProcessor::SetupInputTensor(ov::preprocess::PrePostProcessor& ppp) {
    auto& input_info = ppp.input();
    int h = config_.target_size.first;
    int w = config_.target_size.second;
    // 对于 YUV 格式，需要设置正确的颜色格式
    // 注意：不要手动设置布局，让 OpenVINO 从模型自动推断
    switch (config_.input_format) {
        case ImageFormat::YUV420P: {
            // YUV420P: 单平面格式
            // 对于 YUV/I420/NV12：
            // 必须显式指定：
            // tensor layout
            // tensor shape
            // element type
            input_info.tensor().set_element_type(ov::element::u8)
                                .set_layout("NHWC")
                                .set_spatial_static_shape(h * 3 / 2, w)
                                .set_color_format(ov::preprocess::ColorFormat::I420_SINGLE_PLANE);
            LOG_MAIN_DEBUG_AT("Input format: I420_SINGLE_PLANE (YUV420P)");
            break;
        }    
        case ImageFormat::NV12: {
            // NV12: 单平面格式
            input_info.tensor().set_element_type(ov::element::u8)
                                .set_layout("NHWC")
                                .set_spatial_static_shape(h * 3 / 2, w)
                                .set_color_format(ov::preprocess::ColorFormat::NV12_SINGLE_PLANE);
            LOG_MAIN_DEBUG_AT("Input format: NV12_SINGLE_PLANE");
            break;
        }    
        case ImageFormat::NV21: {
            // NV21: 使用 NV12 近似
            input_info.tensor().set_element_type(ov::element::u8)
                                .set_layout("NHWC")
                                .set_spatial_static_shape(h * 3 / 2, w)
                                .set_color_format(ov::preprocess::ColorFormat::NV12_SINGLE_PLANE);
            LOG_MAIN_DEBUG_AT("Input format: NV12_SINGLE_PLANE (NV21 approximated)");
            break;
        } 
        case ImageFormat::RGB: {
            input_info.tensor().set_element_type(ov::element::u8)
                                .set_layout("NHWC")                                
                                .set_color_format(ov::preprocess::ColorFormat::RGB);
            LOG_MAIN_DEBUG_AT("Input format: RGB");
            break;
        } 
        case ImageFormat::BGR: {
            input_info.tensor().set_element_type(ov::element::u8)
                                .set_layout("NHWC")                                
                                .set_color_format(ov::preprocess::ColorFormat::BGR);
            LOG_MAIN_DEBUG_AT("Input format: BGR");
            break;
        }
        case ImageFormat::GRAY: {
            input_info.tensor().set_element_type(ov::element::u8)
                                .set_layout("NHWC")
                                .set_color_format(ov::preprocess::ColorFormat::GRAY);
            LOG_MAIN_DEBUG_AT("Input format: GRAY");
            break;
        }  
        default: {
            LOG_MAIN_WARN_AT("Unknown input format");
            break;
        }
    }
}

void PrePostProcessor::SetupModelLayout(ov::preprocess::PrePostProcessor& ppp)
{
    auto& input = ppp.input();
    if (config_.layout == "NCHW") {
        ov::Layout layout("NCHW");
        input.model().set_layout(layout);
    } else if (config_.layout == "NHWC") {
        ov::Layout layout("NHWC");
        input.model().set_layout(layout);
    } else {
        LOG_MAIN_WARN_AT("Unknown layout: {}", config_.layout);
    }
    LOG_MAIN_DEBUG_AT("Model layout: {}", config_.layout);
}

void PrePostProcessor::SetupColorConversion(ov::preprocess::PrePostProcessor& ppp) {
    auto& input_info = ppp.input();
    
    // 确定目标颜色格式（模型期望的格式）
    ov::preprocess::ColorFormat target_format;
    if (config_.model_expected_format == ImageFormat::BGR) {
        target_format = ov::preprocess::ColorFormat::BGR;
        LOG_MAIN_DEBUG_AT("Model expects BGR format");
    } else {
        target_format = ov::preprocess::ColorFormat::RGB;
        LOG_MAIN_DEBUG_AT("Model expects RGB format");
    }
    
    // 根据输入格式设置颜色转换
    switch (config_.input_format) {
        case ImageFormat::RGB: {
            if (target_format == ov::preprocess::ColorFormat::BGR) {
                // RGB -> BGR
                input_info.preprocess().convert_color(ov::preprocess::ColorFormat::BGR);
                LOG_MAIN_DEBUG_AT("Color conversion: RGB -> BGR");
            } else {
                LOG_MAIN_DEBUG_AT("No color conversion needed (RGB -> RGB)");
            }
            break;
        }
        case ImageFormat::BGR: {
            if (target_format == ov::preprocess::ColorFormat::RGB) {
                // BGR -> RGB
                input_info.preprocess().convert_color(ov::preprocess::ColorFormat::RGB);
                LOG_MAIN_DEBUG_AT("Color conversion: BGR -> RGB");
            } else {
                LOG_MAIN_DEBUG_AT("No color conversion needed (BGR -> BGR)");
            }
            break;
        }   
        case ImageFormat::NV12: {
            // NV12 -> 目标格式
            input_info.preprocess().convert_color(target_format);
            LOG_MAIN_DEBUG_AT("Color conversion: NV12 -> {}", 
                target_format == ov::preprocess::ColorFormat::RGB ? "RGB" : "BGR");
            break;
        }
        case ImageFormat::NV21: {
            // NV21 -> 目标格式 (OpenVINO 不直接支持 NV21，使用 NV12 作为近似)
            input_info.preprocess().convert_color(target_format);
            LOG_MAIN_DEBUG_AT("Color conversion: NV21 (as NV12) -> {}", 
                target_format == ov::preprocess::ColorFormat::RGB ? "RGB" : "BGR");
            break;
        }
        case ImageFormat::YUV420P: {
            // YUV420P (I420) -> 目标格式
            input_info.preprocess().convert_color(target_format);
            LOG_MAIN_DEBUG_AT("Color conversion: YUV420P -> {}", 
                target_format == ov::preprocess::ColorFormat::RGB ? "RGB" : "BGR");
            break;
        }
        case ImageFormat::GRAY: {
            // 灰度不需要颜色转换
            LOG_MAIN_DEBUG_AT("No color conversion for GRAY");
            break;
        }
        default: {
            LOG_MAIN_WARN_AT("Unknown input format, skipping color conversion");
            break;
        }
    }
}

void PrePostProcessor::SetupDataType(ov::preprocess::PrePostProcessor& ppp)
{
    auto& input = ppp.input();

    if (config_.dtype == "f32") {
        input.preprocess().convert_element_type(ov::element::f32);
        LOG_MAIN_DEBUG_AT("Convert element type: u8 -> f32");
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

