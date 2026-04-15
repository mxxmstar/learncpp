"""
算法模块包 - 所有算法实现的集合

目录结构:
algorithms/
├── base_algorithm.py          # 算法基类
├── mock/                      # Mock 算法（测试用）
│   └── mock_algorithm.py
└── yolov5/                    # YOLOv5 算法（生产用）
    ├── detector.py            # YOLOv5 检测器
    ├── preprocessor.py        # 预处理器
    ├── postprocessor.py       # 后处理器
    ├── utils.py               # 工具函数
    └── yolov5_algorithm.py    # 算法适配器
"""

from algorithms.base_algorithm import BaseAlgorithm, BoundingBox, DetectionResult
from algorithms.mock.mock_algorithm import MockAlgorithm

# 尝试导入 YOLOv5 算法
try:
    from algorithms.yolov5.yolov5_algorithm import YOLOv5Algorithm
    YOLOV5_AVAILABLE = True
except ImportError as e:
    print(f"[Warning] YOLOv5 algorithm not available: {e}")
    YOLOV5_AVAILABLE = False

__all__ = [
    'BaseAlgorithm',
    'BoundingBox', 
    'DetectionResult',
    'MockAlgorithm'
]

if YOLOV5_AVAILABLE:
    __all__.append('YOLOv5Algorithm')
