"""
Mock 算法 - 用于测试的模拟算法
"""

import random
import numpy as np
import sys
import os

# 添加父目录到路径
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from base_algorithm import BaseAlgorithm, BoundingBox, DetectionResult


class MockAlgorithm(BaseAlgorithm):
    """
    模拟检测算法
    
    生成随机的检测框，用于测试 gRPC 通信和系统架构
    """
    
    def __init__(self, max_boxes: int = 3, class_names: list = None):
        super().__init__(name="MockAlgorithm")
        self.max_boxes = max_boxes
        self.class_names = class_names or ['person', 'car', 'dog', 'cat', 'bird']
    
    def initialize(self) -> bool:
        """初始化算法"""
        print("[MockAlgorithm] Initialized")
        self.is_initialized = True
        return True
    
    def process(self, image: np.ndarray, frame_id: str = "") -> DetectionResult:
        """
        处理帧（生成随机检测框）
        
        Args:
            image: 输入图像
            frame_id: 帧ID
            
        Returns:
            DetectionResult: 检测结果
        """
        h, w = image.shape[:2]
        
        # 生成随机数量的检测框
        num_boxes = random.randint(0, self.max_boxes)
        boxes = []
        
        for _ in range(num_boxes):
            box_w = random.randint(50, 150)
            box_h = random.randint(50, 150)
            x = random.randint(0, max(0, w - box_w))
            y = random.randint(0, max(0, h - box_h))
            
            box = BoundingBox(
                x=float(x),
                y=float(y),
                width=float(box_w),
                height=float(box_h),
                class_name=random.choice(self.class_names),
                confidence=random.uniform(0.5, 0.95),
                class_id=random.randint(0, len(self.class_names) - 1)
            )
            boxes.append(box)
        
        # 构建结果
        result = DetectionResult(
            frame_id=frame_id,
            boxes=boxes,
            processing_time_ms=0,  # 由控制器设置
            algorithm="Mock",
            metadata={
                'image_size': f"{w}x{h}",
                'num_boxes': str(num_boxes)
            }
        )
        
        return result
    
    def cleanup(self):
        """清理资源"""
        print("[MockAlgorithm] Cleaned up")
        self.is_initialized = False
