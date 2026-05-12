#include "postprocess/yolov5_postprocessor.h"
#include "common/log/logmanager.h"
#include <algorithm>
#include <cmath>

YOLOv5PostProcessor::YOLOv5PostProcessor(const YOLOv5PostProcessorConfig& config)
    : config_(config) {
    LOG_MAIN_INFO_AT("Creating YOLOv5PostProcessor:");
    LOG_MAIN_INFO_AT("  conf_threshold: {}", config_.conf_threshold);
    LOG_MAIN_INFO_AT("  nms_threshold: {}", config_.nms_threshold);
    LOG_MAIN_INFO_AT("  num_classes: {}", config_.num_classes);
    LOG_MAIN_INFO_AT("  input_size: {}x{}", config_.input_width, config_.input_height);
}

std::vector<Detection> YOLOv5PostProcessor::Process(const ov::Tensor& output_tensor, int original_width, int original_height) {    
    std::vector<Detection> detections;
    
    const float* data = output_tensor.data<const float>();
    if (!data) {
        LOG_MAIN_ERROR_AT("Failed to get output tensor data");
        return detections;
    }
    
    auto shape = output_tensor.get_shape();
    
    // [batch, num_boxes, 5+num_classes]
    int num_boxes = static_cast<int>(shape[1]);
    int dimensions = static_cast<int>(shape[2]);
    
    // 计算 letterbox 缩放因子和 padding
    // letterbox 保持宽高比，将图像缩放至模型输入尺寸，剩余空间用灰色填充
    float scale = std::min(
        static_cast<float>(config_.input_width) / original_width,
        static_cast<float>(config_.input_height) / original_height);
    
    float pad_x = (config_.input_width - original_width * scale) / 2.0f;
    float pad_y = (config_.input_height - original_height * scale) / 2.0f;
    
    LOG_MAIN_DEBUG_AT("Processing {} predictions, scale: {}, pad: ({}, {})",
        num_boxes, scale, pad_x, pad_y);
    // row[0] - 边界框中心点x坐标 (cx)
    // row[1] - 边界框中心点y坐标 (cy)
    // row[2] - 边界框宽度 (w)
    // row[3] - 边界框高度 (h)
    // row[4] - Objectness Score（表示该区域包含目标的可能性）
    // row[5-num_classes] - 类别概率（每个类别概率）
    for (int i = 0; i < num_boxes; ++i) {
        const float* row = data + i * dimensions;
        
        float obj_conf = row[4];
        
        // 忽略置信度小于阈值的预测结果
        if (obj_conf < config_.conf_threshold)
            continue;
        
        // 找出概率最大的类别
        int class_id = -1;
        float class_score = 0.f;
        
        // 遍历类别概率，找出概率最大的类别
        for (int c = 0; c < config_.num_classes; ++c) {
            float score = row[5 + c];
            if (score > class_score) {
                class_score = score;
                class_id = c;
            }
        }
        
        // 获取类别概率最大的类别置信度
        float confidence = obj_conf * class_score;
        
        if (confidence < config_.conf_threshold)
            continue;
        
        // 解析边界框中心点和尺寸
        float cx = row[0];
        float cy = row[1];
        float w = row[2];
        float h = row[3];
        
        // 转换为左上角和右下角坐标
        float x1 = cx - w / 2.f;
        float y1 = cy - h / 2.f;
        float x2 = cx + w / 2.f;
        float y2 = cy + h / 2.f;
        
        // 逆转 letterbox 变换：
        // 1. 减去 padding（还原到原始图像坐标系）
        x1 -= pad_x;
        y1 -= pad_y;
        x2 -= pad_x;
        y2 -= pad_y;
        
        // 2. 除以缩放因子（还原到原始图像尺寸）
        x1 /= scale;
        y1 /= scale;
        x2 /= scale;
        y2 /= scale;
        
        // 裁剪到图像边界内
        x1 = std::max(0.f, std::min(x1, static_cast<float>(original_width - 1)));
        y1 = std::max(0.f, std::min(y1, static_cast<float>(original_height - 1)));
        x2 = std::max(0.f, std::min(x2, static_cast<float>(original_width - 1)));
        y2 = std::max(0.f, std::min(y2, static_cast<float>(original_height - 1)));
        
        Detection det;
        det.class_id = class_id;
        det.confidence = confidence;
        det.x1 = x1;
        det.y1 = y1;
        det.x2 = x2;
        det.y2 = y2;
        
        detections.push_back(det);
    }
    
    LOG_MAIN_DEBUG_AT("Extracted {} detections before NMS", detections.size());
    
    NMS(detections);
    
    LOG_MAIN_DEBUG_AT("Final {} detections after NMS", detections.size());
    
    return detections;
}

float YOLOv5PostProcessor::IoU(const Detection& a, const Detection& b) {
    // 计算两个边界框的交集区域
    float x1 = std::max(a.x1, b.x1);
    float y1 = std::max(a.y1, b.y1);
    float x2 = std::min(a.x2, b.x2);
    float y2 = std::min(a.y2, b.y2);
    
    // 计算交集面积
    float intersection_w = std::max(0.0f, x2 - x1);
    float intersection_h = std::max(0.0f, y2 - y1);
    float intersection_area = intersection_w * intersection_h;
    
    // 计算各自面积
    float area_a = a.Area();
    float area_b = b.Area();
    
    // 计算并集面积
    float union_area = area_a + area_b - intersection_area;
    
    // 避免除零
    if (union_area <= 0.0f) {
        return 0.0f;
    }
    
    // 计算 IoU
    return intersection_area / union_area;
}

void YOLOv5PostProcessor::NMS(std::vector<Detection>& detections) {
    if (detections.empty()) {
        return;
    }
    
    // 按置信度降序排序
    std::sort(detections.begin(), detections.end(),
        [](const Detection& a, const Detection& b) {
            return a.confidence > b.confidence;
        });
    
    std::vector<Detection> keep;
    std::vector<bool> suppressed(detections.size(), false);
    
    // 遍历所有检测框
    for (size_t i = 0; i < detections.size(); ++i) {
        if (suppressed[i]) {
            continue;
        }
        
        // 保留当前高置信度框
        keep.push_back(detections[i]);
        
        // 抑制与当前框 IoU 超过阈值的低置信度框
        for (size_t j = i + 1; j < detections.size(); ++j) {
            if (suppressed[j]) {
                continue;
            }
            
            // 只对同类别的框进行 NMS
            // 或者可以对所有框进行 NMS（注释下面这行）
            if (detections[i].class_id != detections[j].class_id) {
                continue;
            }
            
            // 计算 IoU
            float iou = IoU(detections[i], detections[j]);
            
            // 如果 IoU 超过阈值，抑制该框
            if (iou > config_.nms_threshold) {
                suppressed[j] = true;
            }
        }
    }
    
    // 更新检测结果
    detections = std::move(keep);
}

std::pair<float, float> YOLOv5PostProcessor::CalculateScale(
    int original_width,
    int original_height) const {
    
    float scale_x = static_cast<float>(original_width) / config_.input_width;
    float scale_y = static_cast<float>(original_height) / config_.input_height;
    
    return {scale_x, scale_y};
}
