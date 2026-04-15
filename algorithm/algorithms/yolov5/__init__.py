"""
YOLOv5 算法模块 - 基于 OpenVINO 实现

包含:
- YOLOv5AsyncDetector: 异步检测器（高性能）
- YOLOv5SyncDetector: 同步检测器（简单）
- LetterboxPreprocessor: Letterbox 预处理器
- YOLOv5PostProcessor: NMS 后处理器
- YOLOv5Algorithm: 继承 BaseAlgorithm 的适配器
"""

from algorithms.yolov5.detector import YOLOv5AsyncDetector, YOLOv5SyncDetector
from algorithms.yolov5.preprocessor import LetterboxPreprocessor
from algorithms.yolov5.postprocessor import YOLOv5PostProcessor
from algorithms.yolov5.yolov5_algorithm import YOLOv5Algorithm

__all__ = [
    'YOLOv5AsyncDetector',
    'YOLOv5SyncDetector',
    'LetterboxPreprocessor',
    'YOLOv5PostProcessor',
    'YOLOv5Algorithm'
]
