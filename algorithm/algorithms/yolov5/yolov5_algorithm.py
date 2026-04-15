"""
YOLOv5 算法实现 - 继承 BaseAlgorithm
"""

import sys
import os
import numpy as np
from typing import List

# 添加父目录到路径
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from base_algorithm import BaseAlgorithm, BoundingBox, DetectionResult

# 导入 YOLOv5 检测器
try:
    from detector import YOLOv5AsyncDetector, YOLOv5SyncDetector
    YOLOV5_AVAILABLE = True
except ImportError:
    print("[Warning] YOLOv5 detector not available")
    YOLOV5_AVAILABLE = False


class YOLOv5Algorithm(BaseAlgorithm):
    """
    YOLOv5 目标检测算法
    
    基于 OpenVINO 实现的 YOLOv5 检测器
    """
    
    def __init__(self, model_path: str = None, device: str = "CPU", 
                 async_mode: bool = True, num_requests: int = 4,
                 conf_threshold: float = 0.25, iou_threshold: float = 0.45):
        """
        初始化 YOLOv5 算法
        
        Args:
            model_path: OpenVINO IR 模型路径 (.xml 文件)
            device: 推理设备 ("CPU", "GPU" 等)
            async_mode: 是否使用异步模式
            num_requests: 异步请求数量
            conf_threshold: 置信度阈值
            iou_threshold: NMS IoU 阈值
        """
        super().__init__(name="YOLOv5Algorithm")
        self.model_path = model_path
        self.device = device
        self.async_mode = async_mode
        self.num_requests = num_requests
        self.conf_threshold = conf_threshold
        self.iou_threshold = iou_threshold
        self.detector = None
    
    def initialize(self) -> bool:
        """初始化 YOLOv5 检测器"""
        if not YOLOV5_AVAILABLE:
            print("[YOLOv5Algorithm] YOLOv5 module not available")
            return False
        
        if not self.model_path:
            print("[YOLOv5Algorithm] Model path not provided")
            return False
        
        if not os.path.exists(self.model_path):
            print(f"[YOLOv5Algorithm] Model file not found: {self.model_path}")
            return False
        
        try:
            print(f"[YOLOv5Algorithm] Loading model from {self.model_path}")
            print(f"[YOLOv5Algorithm] Device: {self.device}, Async: {self.async_mode}")
            
            if self.async_mode:
                # 使用异步检测器（高性能）
                self.detector = YOLOv5AsyncDetector(
                    model_path=self.model_path,
                    device=self.device,
                    num_requests=self.num_requests
                )
            else:
                # 使用同步检测器（简单）
                self.detector = YOLOv5SyncDetector(
                    model_path=self.model_path,
                    device=self.device
                )
            
            self.is_initialized = True
            print("[YOLOv5Algorithm] Model loaded successfully")
            return True
            
        except Exception as e:
            print(f"[YOLOv5Algorithm] Failed to load model: {e}")
            import traceback
            traceback.print_exc()
            return False
    
    def process(self, image: np.ndarray, frame_id: str = "") -> DetectionResult:
        """
        处理帧（YOLOv5 检测）
        
        Args:
            image: 输入图像 (BGR格式)
            frame_id: 帧ID
            
        Returns:
            DetectionResult: 检测结果
        """
        if not self.is_initialized or not self.detector:
            print("[YOLOv5Algorithm] Detector not initialized")
            return DetectionResult(
                frame_id=frame_id,
                boxes=[],
                processing_time_ms=0,
                algorithm="YOLOv5"
            )
        
        try:
            # 调用检测器
            if self.async_mode:
                # 异步模式
                detections = self.detector.detect_async(image)
            else:
                # 同步模式
                detections = self.detector.detect(image)
            
            # 转换为 BoundingBox 列表
            boxes = []
            for det in detections:
                box = BoundingBox(
                    x=float(det['x']),
                    y=float(det['y']),
                    width=float(det['width']),
                    height=float(det['height']),
                    class_name=det.get('class_name', 'unknown'),
                    confidence=float(det['confidence']),
                    class_id=int(det.get('class_id', 0))
                )
                boxes.append(box)
            
            # 构建结果
            result = DetectionResult(
                frame_id=frame_id,
                boxes=boxes,
                processing_time_ms=0,  # 由控制器设置
                algorithm="YOLOv5",
                metadata={
                    'model': self.model_path,
                    'device': self.device,
                    'async_mode': str(self.async_mode),
                    'num_detections': str(len(boxes))
                }
            )
            
            return result
            
        except Exception as e:
            print(f"[YOLOv5Algorithm] Detection error: {e}")
            import traceback
            traceback.print_exc()
            return DetectionResult(
                frame_id=frame_id,
                boxes=[],
                processing_time_ms=0,
                algorithm="YOLOv5"
            )
    
    def cleanup(self):
        """清理资源"""
        if self.detector:
            try:
                # 清理检测器资源
                del self.detector
                self.detector = None
            except Exception as e:
                print(f"[YOLOv5Algorithm] Cleanup error: {e}")
        
        self.is_initialized = False
        print("[YOLOv5Algorithm] Cleaned up")
