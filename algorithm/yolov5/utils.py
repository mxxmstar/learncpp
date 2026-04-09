"""
YOLOv5 工具函数

提供可视化和辅助功能：
- 绘制检测框
- 加载类别名称
- 颜色生成
"""

import cv2
import numpy as np
from typing import List, Dict, Optional


# COCO 数据集的 80 个类别
COCO_CLASSES = [
    'person', 'bicycle', 'car', 'motorcycle', 'airplane', 'bus', 'train', 'truck', 'boat',
    'traffic light', 'fire hydrant', 'stop sign', 'parking meter', 'bench', 'bird', 'cat',
    'dog', 'horse', 'sheep', 'cow', 'elephant', 'bear', 'zebra', 'giraffe', 'backpack',
    'umbrella', 'handbag', 'tie', 'suitcase', 'frisbee', 'skis', 'snowboard', 'sports ball',
    'kite', 'baseball bat', 'baseball glove', 'skateboard', 'surfboard', 'tennis racket',
    'bottle', 'wine glass', 'cup', 'fork', 'knife', 'spoon', 'bowl', 'banana', 'apple',
    'sandwich', 'orange', 'broccoli', 'carrot', 'hot dog', 'pizza', 'donut', 'cake', 'chair',
    'couch', 'potted plant', 'bed', 'dining table', 'toilet', 'tv', 'laptop', 'mouse',
    'remote', 'keyboard', 'cell phone', 'microwave', 'oven', 'toaster', 'sink', 'refrigerator',
    'book', 'clock', 'vase', 'scissors', 'teddy bear', 'hair drier', 'toothbrush'
]


def load_class_names(class_file: str = None) -> List[str]:
    """加载类别名称
    
    参数:
        class_file: 类别文件路径（每行一个类别名）
                   如果为 None，使用 COCO 类别
        
    返回:
        list: 类别名称列表
    """
    if class_file is not None:
        with open(class_file, 'r') as f:
            return [line.strip() for line in f.readlines()]
    
    return COCO_CLASSES


def generate_colors(num_classes: int) -> List[tuple]:
    """为每个类别生成唯一颜色
    
    参数:
        num_classes: 类别数量
        
    返回:
        list: 颜色列表，每个元素为 (B, G, R) 元组
    """
    # 使用 HSV 色彩空间生成均匀分布的颜色
    colors = []
    for i in range(num_classes):
        hue = int(i * 180 / num_classes)
        saturation = 255
        value = 255
        
        # 转换到 BGR
        hsv = np.uint8([[[hue, saturation, value]]])
        bgr = cv2.cvtColor(hsv, cv2.COLOR_HSV2BGR)[0][0]
        
        colors.append(tuple(int(x) for x in bgr))
    
    return colors


def draw_detections(image: np.ndarray, detections: List[Dict], 
                    class_names: List[str] = None,
                    show_confidence: bool = True,
                    line_thickness: int = 2,
                    font_scale: float = 0.5) -> np.ndarray:
    """在图像上绘制检测结果
    
    参数:
        image: 输入图像 (H, W, 3)，BGR 格式
        detections: 检测结果列表
        class_names: 类别名称列表
        show_confidence: 是否显示置信度
        line_thickness: 边框线宽
        font_scale: 字体大小
        
    返回:
        numpy array: 绘制后的图像
    """
    # 复制图像（避免修改原图）
    result_image = image.copy()
    
    # 如果没有提供类别名称，使用默认索引
    if class_names is None:
        class_names = [str(i) for i in range(80)]
    
    # 生成颜色
    num_classes = len(class_names)
    colors = generate_colors(num_classes)
    
    # 绘制每个检测框
    for det in detections:
        x1 = int(det['x1'])
        y1 = int(det['y1'])
        x2 = int(det['x2'])
        y2 = int(det['y2'])
        confidence = det['confidence']
        class_id = det['class_id']
        
        # 获取颜色
        color = colors[class_id % num_classes]
        
        # 绘制边界框
        cv2.rectangle(result_image, (x1, y1), (x2, y2), color, line_thickness)
        
        # 准备标签文本
        class_name = class_names[class_id] if class_id < len(class_names) else str(class_id)
        if show_confidence:
            label = f"{class_name}: {confidence:.2f}"
        else:
            label = class_name
        
        # 计算文本大小
        (text_width, text_height), baseline = cv2.getTextSize(
            label, cv2.FONT_HERSHEY_SIMPLEX, font_scale, line_thickness
        )
        
        # 绘制文本背景
        cv2.rectangle(
            result_image,
            (x1, y1 - text_height - baseline),
            (x1 + text_width, y1),
            color,
            cv2.FILLED
        )
        
        # 绘制文本
        cv2.putText(
            result_image,
            label,
            (x1, y1 - baseline),
            cv2.FONT_HERSHEY_SIMPLEX,
            font_scale,
            (255, 255, 255),
            line_thickness
        )
    
    return result_image


def draw_fps(image: np.ndarray, fps: float, position: tuple = (10, 30),
             font_scale: float = 1.0, color: tuple = (0, 255, 0),
             thickness: int = 2) -> np.ndarray:
    """在图像上绘制 FPS
    
    参数:
        image: 输入图像
        fps: FPS 值
        position: 文本位置 (x, y)
        font_scale: 字体大小
        color: 文本颜色 (B, G, R)
        thickness: 线宽
        
    返回:
        numpy array: 绘制后的图像
    """
    result_image = image.copy()
    
    label = f"FPS: {fps:.1f}"
    
    cv2.putText(
        result_image,
        label,
        position,
        cv2.FONT_HERSHEY_SIMPLEX,
        font_scale,
        color,
        thickness
    )
    
    return result_image
