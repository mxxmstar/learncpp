#pragma once

#include "alg/inference/tensor_data.h"
#include <openvino/openvino.hpp>
#include <vector>
#include <string>
#include <memory>

/// @brief 图像格式枚举
enum class ImageFormat {
    RGB,        ///< RGB 三通道
    BGR,        ///< BGR 三通道（OpenCV 默认）
    NV12,       ///< Y + UV 交错（半采样）
    NV21,       ///< Y + VU 交错（半采样）
    YUV420P,    ///< Y + U + V 三个独立平面
    GRAY        ///< 单通道灰度
};

/// @brief 预处理配置
struct PreProcessConfig {
    /// @brief 输入图像真实尺寸
    int input_width = 1920;
    int input_height = 1080;

    /// @brief 模型输入尺寸
    int model_width = 640;
    int model_height = 640;

    /// @brief 输入图像格式
    ImageFormat input_format = ImageFormat::BGR;
    
    /// @brief 模型期望的颜色格式（RGB 或 BGR）
    /// 大多数 YOLO 模型期望 RGB，OpenCV 训练的模型可能期望 BGR
    ImageFormat model_expected_format = ImageFormat::RGB;
    
    /// @brief 是否归一化到 [0, 1]
    bool normalize = true;
    
    /// @brief 归一化均值 (R, G, B)
    std::vector<float> mean = {0.0f, 0.0f, 0.0f};
    
    /// @brief 归一化标准差 (R, G, B)
    std::vector<float> std = {255.0f, 255.0f, 255.0f};
    
    /// @brief 输入布局（模型期望的布局）NCHW 或 NHWC
    std::string layout = "NCHW";
    
    /// @brief 输入数据类型: f32 或 u8
    std::string dtype = "f32";
};

/// @brief OpenVINO 预处理器
/// 
/// 使用 OpenVINO PrePostProcessor API 实现高效的图像预处理，支持：
/// - 颜色空间转换 (RGB/BGR/NV12/NV21/YUV420P → RGB/BGR)
/// - 缩放 (Resize)
/// - 归一化 (Normalize)
/// - 布局转换 (NHWC ↔ NCHW)
/// - 数据类型转换 (UINT8 → FLOAT32)
///
/// 所有操作在 OpenVINO 内部完成，避免额外的内存拷贝和 CPU 计算
class PrePostProcessor {
public:
    PrePostProcessor();
    ~PrePostProcessor();
    
    /// @brief 配置预处理器并应用到模型
    /// @param model OpenVINO 原始模型（未编译）
    /// @param config 预处理配置
    /// @return 应用预处理后的新模型
    std::shared_ptr<ov::Model> Configure(std::shared_ptr<ov::Model> model, const PreProcessConfig& config);
    
    /// @brief 获取配置信息
    const PreProcessConfig& GetConfig() const { return config_; }
    
    /// @brief 检查是否已配置
    bool IsConfigured() const { return configured_; }

private:
    /// @brief 设置输入 Tensor 信息
    void SetupInputTensor(ov::preprocess::PrePostProcessor& ppp);

    /// @brief 设置模型输入layout（布局和数据类型）
    void SetupModelLayout(ov::preprocess::PrePostProcessor& ppp);
    
    /// @brief 设置颜色空间转换
    void SetupColorConversion(ov::preprocess::PrePostProcessor& ppp);
    
    /// @brief 配置数据类型转换
    void SetupDataType(ov::preprocess::PrePostProcessor& ppp);

    /// @brief 设置缩放
    void SetupResize(ov::preprocess::PrePostProcessor& ppp);
    
    /// @brief 设置归一化
    void SetupNormalization(ov::preprocess::PrePostProcessor& ppp);    
    
    // ==================== 成员变量 ====================
    /// @brief 预处理配置
    PreProcessConfig config_;
    
    /// @brief 是否已配置
    bool configured_ = false;
};
