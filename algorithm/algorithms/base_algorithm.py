"""
算法基类 - 所有算法的抽象接口
"""

from abc import ABC, abstractmethod
from typing import List, Dict, Any, Optional
import numpy as np


class BoundingBox:
    """检测框"""
    def __init__(self, x: float, y: float, width: float, height: float,
                 class_name: str, confidence: float, class_id: int = 0):
        self.x = x
        self.y = y
        self.width = width
        self.height = height
        self.class_name = class_name
        self.confidence = confidence
        self.class_id = class_id
    
    def to_dict(self) -> Dict[str, Any]:
        return {
            'x': self.x,
            'y': self.y,
            'width': self.width,
            'height': self.height,
            'class_name': self.class_name,
            'confidence': self.confidence,
            'class_id': self.class_id
        }


class DetectionResult:
    """检测结果"""
    def __init__(self, frame_id: str, boxes: List[BoundingBox],
                 processing_time_ms: int, algorithm: str,
                 metadata: Optional[Dict[str, str]] = None):
        self.frame_id = frame_id
        self.boxes = boxes
        self.processing_time_ms = processing_time_ms
        self.algorithm = algorithm
        self.metadata = metadata or {}


class BaseAlgorithm(ABC):
    """
    算法基类
    
    所有具体算法都需要继承此类并实现 process 方法
    """
    
    def __init__(self, name: str = "BaseAlgorithm"):
        self.name = name
        self.is_initialized = False
    
    @abstractmethod
    def initialize(self) -> bool:
        """
        初始化算法
        
        Returns:
            bool: 初始化成功返回 True
        """
        pass
    
    @abstractmethod
    def process(self, image: np.ndarray, frame_id: str = "") -> DetectionResult:
        """
        处理单帧图像
        
        Args:
            image: 输入图像 (BGR格式)
            frame_id: 帧ID
            
        Returns:
            DetectionResult: 检测结果
        """
        pass
    
    @abstractmethod
    def cleanup(self):
        """清理资源"""
        pass
    
    def get_name(self) -> str:
        """获取算法名称"""
        return self.name
    
    def is_available(self) -> bool:
        """检查算法是否可用"""
        return self.is_initialized
