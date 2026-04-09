"""
YOLOv5 OpenVINO 异步检测器模块

提供高性能的 YOLOv5 目标检测功能，支持：
- 异步推理（非阻塞）
- 批量处理
- 视频流实时检测
- 多请求并发

工作流程：
┌─────────────┐
│  预处理图片  │
└──────┬──────┘
       ↓
┌─────────────┐
│  设置输入    │
└──────┬──────┘
       ↓
┌─────────────┐
│启动异步推理  │ ← 立即返回！不等待
│(start_async)│   CPU 可以继续做其他事
└──────┬──────┘
       ↓
┌─────────────┐
│ 做其他事情   │ ← 可以预处理下一张、显示结果等
│ (可选)      │
└──────┬──────┘
       ↓
┌─────────────┐
│ 等待并获取  │ ← 需要时才等待
└──────┬──────┘
       ↓
┌─────────────┐
│  返回结果    │
└─────────────┘
"""

from .detector import YOLOv5AsyncDetector, YOLOv5SyncDetector
from .preprocessor import LetterboxPreprocessor
from .postprocessor import YOLOv5PostProcessor
from .utils import draw_detections, load_class_names

__all__ = [
    'YOLOv5AsyncDetector',
    'YOLOv5SyncDetector', 
    'LetterboxPreprocessor',
    'YOLOv5PostProcessor',
    'draw_detections',
    'load_class_names'
]
