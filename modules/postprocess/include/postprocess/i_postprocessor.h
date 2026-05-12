#pragma once
#include <openvino/openvino.hpp>
#include <vector>
#include <memory>

/// @brief 检测结果结构体
/// 
/// 表示单个目标检测框，包含类别、置信度和边界框坐标
struct Detection {
    /// @brief 类别 ID（-1 表示无效/背景）
    int class_id = -1;
    
    /// @brief 置信度分数 [0, 1]
    float confidence = 0.f;
    
    /// @brief 边界框左上角坐标
    float x1 = 0.f;
    float y1 = 0.f;
    
    /// @brief 边界框右下角坐标
    float x2 = 0.f;
    float y2 = 0.f;
    
    /// @brief 获取边界框宽度
    /// @return x2 - x1
    float Width() const { return x2 - x1; }
    
    /// @brief 获取边界框高度
    /// @return y2 - y1
    float Height() const { return y2 - y1; }
    
    /// @brief 计算边界框面积
    /// @return (x2 - x1) * (y2 - y1)
    float Area() const { return Width() * Height(); }
};

/// @brief 后处理器接口基类
/// 
/// 定义模型后处理的标准接口，不同模型可实现不同的后处理逻辑。
/// 支持的模型类型：
/// - YOLOv5/YOLOv8 系列
/// - SSD 系列
/// - 其他检测模型
///
/// @note 所有子类必须实现 Process() 纯虚函数
class PostProcessor {
public:
    virtual ~PostProcessor() = default;
    
    /// @brief 处理模型输出张量，将模型推理的原始输出转换为检测结果向量
    /// @param output_tensor 模型输出的 OpenVINO 张量
    /// @param original_width 原始图像宽度（用于坐标映射）
    /// @param original_height 原始图像高度（用于坐标映射）
    /// @return 检测结果向量，按置信度降序排列
    virtual std::vector<Detection> Process(const ov::Tensor& output_tensor, int original_width, int original_height) = 0;
    
    /// @brief 获取后处理器的名称
    /// @return 处理器名称
    virtual const char* GetName() const = 0;
    
    /// @brief 重置内部状态（如需要）
    virtual void Reset() {}
};

