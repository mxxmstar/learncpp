#pragma once
#include <openvino/openvino.hpp>
#include <vector>
#include <memory>
#include "postprocess/i_postprocessor.h"

const std::vector<std::string> class_names = {
    "person", "bicycle", "car", "motorcycle", "airplane", "bus", "train", "truck", "boat", "traffic light",
    "fire hydrant", "stop sign", "parking meter", "bench", "bird", "cat", "dog", "horse", "sheep", "cow",
    "elephant", "bear", "zebra", "giraffe", "backpack", "umbrella", "handbag", "tie", "suitcase", "frisbee",
    "skis", "snowboard", "sports ball", "kite", "baseball bat", "baseball glove", "skateboard", "surfboard",
    "tennis racket", "bottle", "wine glass", "cup", "fork", "knife", "spoon", "bowl", "banana", "apple",
    "sandwich", "orange", "broccoli", "carrot", "hot dog", "pizza", "donut", "cake", "chair", "couch",
    "potted plant", "bed", "dining table", "toilet", "tv", "laptop", "mouse", "remote", "keyboard", "cell phone",
    "microwave", "oven", "toaster", "sink", "refrigerator", "book", "clock", "vase", "scissors", "teddy bear",
    "hair drier", "toothbrush"
};


/// @brief YOLOv5 后处理器配置
struct YOLOv5PostProcessorConfig {
    /// @brief 置信度阈值，低于此值的检测框将被过滤
    float conf_threshold = 0.25f;
    
    /// @brief NMS（非极大值抑制）阈值，用于去除重叠框
    float nms_threshold = 0.45f;
    
    /// @brief 模型检测的类别数量
    int num_classes = 80;
    
    /// @brief 模型输入宽度（必须是 32 的倍数，如 320/416/608/640/1280）
    int input_width = 640;
    
    /// @brief 模型输入高度
    int input_height = 640;
};

/// @brief YOLOv5 模型后处理器
/// 
/// 实现 YOLOv5 系列模型（YOLOv5s/m/l/x）的后处理逻辑。
/// 
/// ### YOLOv5 输出格式
/// YOLOv5 输出形状为 [1, 84000, 85]，其中 85 = 4(box) + 1(conf) + 80(classes)
/// - 前 4 个值：边界框 (x_center, y_center, width, height)
/// - 第 5 个值：置信度分数
/// - 后 80 个值：各类别的概率
/// 
/// ### 坐标映射
/// 模型输出的边界框基于模型输入尺寸 (input_width x input_height)。
/// 通过 scale因子 将坐标映射回原始图像尺寸。
/// 
/// ### 处理流程
/// 1. 遍历所有预测框
/// 2. 检查置信度是否超过阈值
/// 3. 计算各类别的最大概率
/// 4. 如果最大概率 * 置信度 > conf_threshold，保留该检测
/// 5. 对所有保留的检测执行 NMS
/// 6. 返回最终的检测结果
/// 
/// @note 支持 YOLOv5n/s/m/l/x 和 YOLOv8 全系列
class YOLOv5PostProcessor : public PostProcessor {
public:
    /// @brief 构造函数
    /// @param config 后处理器配置参数
    explicit YOLOv5PostProcessor(const YOLOv5PostProcessorConfig& config);
    
    /// @brief 默认析构函数
    ~YOLOv5PostProcessor() override = default;

    /// @brief 处理模型输出
    /// 
    /// 将 YOLOv5 原始输出转换为检测结果
    /// 
    /// @param output_tensor 模型输出张量，形状 [1, num_predictions, 5+num_classes]
    /// @param original_width 原始图像宽度
    /// @param original_height 原始图像高度
    /// @return 检测结果向量
    std::vector<Detection> Process(const ov::Tensor& output_tensor, int original_width, int original_height) override;
    
    /// @brief 获取处理器名称
    /// @return "YOLOv5PostProcessor"
    const char* GetName() const override { return "YOLOv5PostProcessor"; }
    
    /// @brief 获取配置
    /// @return 配置引用
    const YOLOv5PostProcessorConfig& GetConfig() const { return config_; }

private:
    /// @brief 计算两个检测框的 IoU（交并比）
    /// 
    /// IoU = Intersection / Union
    /// 用于 NMS 阶段判断两个框是否重叠
    /// 
    /// @param a 第一个检测框
    /// @param b 第二个检测框
    /// @return IoU 值 [0, 1]
    float IoU(const Detection& a, const Detection& b);
    
    /// @brief 非极大值抑制（NMS）
    /// 
    /// 移除与高置信度框重叠度过高的低置信度框。
    /// 对于每个类别，独立执行 NMS。
    /// 
    /// @param detections 输入检测框向量，函数将直接修改此向量
    void NMS(std::vector<Detection>& detections);
    
    /// @brief 计算坐标映射的缩放因子
    /// 
    /// 计算从模型输入尺寸到原始图像尺寸的缩放比例
    /// 
    /// @param original_width 原始图像宽度
    /// @param original_height 原始图像高度
    /// @return pair (scale_x, scale_y)
    std::pair<float, float> CalculateScale(int original_width, int original_height) const;

private:
    /// @brief 后处理器配置参数
    YOLOv5PostProcessorConfig config_;
};
