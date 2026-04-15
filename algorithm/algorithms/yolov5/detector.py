"""
YOLOv5 OpenVINO 检测器

提供同步和异步两种推理模式：
- YOLOv5SyncDetector: 同步推理（简单直接）
- YOLOv5AsyncDetector: 异步推理（高性能，支持并发）
"""

import openvino as ov
import numpy as np
import cv2
from typing import List, Dict, Optional

try:
    from .preprocessor import LetterboxPreprocessor
    from .postprocessor import YOLOv5PostProcessor
except ImportError:
    from preprocessor import LetterboxPreprocessor
    from postprocessor import YOLOv5PostProcessor


class YOLOv5SyncDetector:
    """YOLOv5 同步检测器
    
    传统的同步推理方式，代码简单但会阻塞等待。
    
    工作流程：
    1. 预处理图片
    2. 设置输入
    3. 执行推理（阻塞等待）
    4. 返回结果
    
    示例：
        >>> detector = YOLOv5SyncDetector("model.xml")
        >>> results = detector.detect(image)
    """
    
    def __init__(self, model_path: str, device: str = "CPU"):
        """初始化同步检测器
        
        参数:
            model_path: OpenVINO IR 模型路径 (.xml 文件)
            device: 推理设备 ("CPU", "GPU", "MYRIAD" 等)
        """
        # 创建 OpenVINO 核心对象
        self.core = ov.Core()
        
        # 读取模型
        self.model = self.core.read_model(model_path)
        
        # 编译模型
        self.compiled_model = self.core.compile_model(self.model, device)
        
        # 创建推理请求
        self.infer_request = self.compiled_model.create_infer_request()
        
        # 获取模型输入形状
        self.input_shape = self.compiled_model.input(0).shape
        self.input_height = self.input_shape[2]
        self.input_width = self.input_shape[3]
        
        # 创建预处理器和后处理器
        self.preprocessor = LetterboxPreprocessor(self.input_width, self.input_height)
        self.postprocessor = YOLOv5PostProcessor()
        
        print(f"[同步检测器] 模型加载成功")
        print(f"  - 输入尺寸: {self.input_width}x{self.input_height}")
        print(f"  - 设备: {device}")
    
    def detect(self, image: np.ndarray, conf_threshold: float = 0.25, 
               iou_threshold: float = 0.45) -> List[Dict]:
        """同步检测
        
        参数:
            image: OpenCV 图片 (H, W, 3)，BGR 格式
            conf_threshold: 置信度阈值
            iou_threshold: NMS IoU 阈值
            
        返回:
            list: 检测结果列表
        """
        # 预处理
        input_data, preprocess_info = self.preprocessor.prepare_for_inference(image)
        
        # 设置输入
        self.infer_request.set_input_tensor(ov.Tensor(input_data))
        
        # 执行推理（阻塞）
        self.infer_request.infer()
        
        # 获取输出
        output = self.infer_request.get_output_tensor().data
        
        # 后处理
        self.postprocessor.conf_threshold = conf_threshold
        self.postprocessor.iou_threshold = iou_threshold
        detections = self.postprocessor.process(output, preprocess_info)
        
        return detections


class YOLOv5AsyncDetector:
    """YOLOv5 异步检测器
    
    高性能的异步推理方式，支持并发处理。
    
    工作流程（非阻塞式）：
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
    
    重要提示：
    ⚠️ 单个 InferRequest 不能同时处理多个任务
    ⚠️ 需要创建多个 InferRequest 实现真正的并发
    
    特点：
    ✓ 可以重叠计算和推理
    ✓ 提高 CPU/GPU 利用率
    ✓ 适合批量处理和视频流
    ✗ 代码稍微复杂一些
    
    示例：
        >>> detector = YOLOv5AsyncDetector("model.xml", num_requests=4)
        >>> request = detector.detect_async(image)  # 立即返回
        >>> # ... 可以做其他事情 ...
        >>> results = detector.get_result(request)  # 需要时才等待
    """
    
    def __init__(self, model_path: str, num_requests: int = 4, device: str = "CPU"):
        """初始化异步检测器
        
        参数:
            model_path: OpenVINO IR 模型路径 (.xml 文件)
            num_requests: 并发推理请求数量
                         - 太少：无法充分利用硬件
                         - 太多：占用过多内存
                         - 推荐：4-8 个（根据硬件调整）
            device: 推理设备 ("CPU", "GPU", "MYRIAD" 等)
        """
        # 创建 OpenVINO 核心对象
        self.core = ov.Core()
        
        # 读取模型
        self.model = self.core.read_model(model_path)
        
        # 编译模型
        self.compiled_model = self.core.compile_model(self.model, device)
        
        # ⚠️ 关键：创建多个异步推理请求
        # 每个请求可以独立处理一个推理任务
        self.num_requests = num_requests
        self.infer_requests = []
        for i in range(num_requests):
            request = self.compiled_model.create_infer_request()
            self.infer_requests.append(request)
        
        # 当前使用的请求索引（轮询策略）
        self.current_request_idx = 0
        
        # 获取模型输入形状
        self.input_shape = self.compiled_model.input(0).shape
        self.input_height = self.input_shape[2]
        self.input_width = self.input_shape[3]
        
        # 创建预处理器和后处理器
        self.preprocessor = LetterboxPreprocessor(self.input_width, self.input_height)
        self.postprocessor = YOLOv5PostProcessor()
        
        # 保存每个请求对应的预处理信息
        self.preprocess_infos = {}
        
        print(f"[异步检测器] 模型加载成功")
        print(f"  - 输入尺寸: {self.input_width}x{self.input_height}")
        print(f"  - 设备: {device}")
        print(f"  - 并发请求数: {num_requests}")
    
    def detect_async(self, image: np.ndarray) -> ov.InferRequest:
        """异步检测：启动推理但不等待
        
        这个方法会：
        1. 找到一个可用的推理请求
        2. 预处理图片
        3. 设置输入
        4. 启动异步推理
        5. 立即返回请求对象（不等待完成）
        
        参数:
            image: OpenCV 图片 (H, W, 3)，BGR 格式
            
        返回:
            InferRequest: 推理请求对象（稍后可以获取结果）
        """
        # Step 1: 选择一个推理请求（轮询策略）
        # 如果有多个图片要处理，轮流使用不同的请求
        request = self.infer_requests[self.current_request_idx]
        self.current_request_idx = (self.current_request_idx + 1) % self.num_requests
        
        # Step 2: 如果这个请求还在忙，先等待它完成
        # （避免覆盖正在进行的推理任务）
        request.wait()
        
        # Step 3: 预处理
        input_data, preprocess_info = self.preprocessor.prepare_for_inference(image)
        
        # 保存预处理信息（用于后续坐标还原）
        request_id = id(request)
        self.preprocess_infos[request_id] = preprocess_info
        
        # Step 4: 设置输入数据
        request.set_input_tensor(ov.Tensor(input_data))
        
        # Step 5: 启动异步推理（非阻塞，立即返回）
        # ⚠️ 关键：这里不会等待推理完成！
        request.start_async()
        
        # 返回请求对象，调用者可以稍后获取结果
        return request
    
    def get_result(self, infer_request: ov.InferRequest, 
                   preprocess_info: dict = None,
                   conf_threshold: float = 0.25,
                   iou_threshold: float = 0.45) -> List[Dict]:
        """获取异步推理的结果
        
        这个方法会：
        1. 等待推理完成（如果还没完成）
        2. 获取输出数据
        3. 后处理并返回结果
        
        参数:
            infer_request: 推理请求对象（从 detect_async 返回）
            preprocess_info: 预处理信息（用于坐标还原），如果为 None 则使用自动保存的信息
            conf_threshold: 置信度阈值
            iou_threshold: NMS IoU 阈值
            
        返回:
            list: 检测结果列表
        """
        # Step 1: 等待推理完成
        # 如果推理已经完成，这个方法会立即返回
        # 如果还在进行，会阻塞等待完成
        infer_request.wait()
        
        # Step 2: 如果没有传入 preprocess_info，则使用自动保存的信息
        if preprocess_info is None:
            request_id = id(infer_request)
            preprocess_info = self.preprocess_infos.pop(request_id, None)
        
        # Step 3: 获取输出结果
        output = infer_request.get_output_tensor().data
        
        # Step 4: 后处理
        self.postprocessor.conf_threshold = conf_threshold
        self.postprocessor.iou_threshold = iou_threshold
        detections = self.postprocessor.process(output, preprocess_info)
        
        return detections
    
    def detect_and_get(self, image: np.ndarray, conf_threshold: float = 0.25,
                       iou_threshold: float = 0.45) -> List[Dict]:
        """同步方式的异步检测（ convenience 方法）
        
        这个方法会：
        1. 启动异步推理
        2. 立即等待结果
        3. 返回检测结果
        
        适用于只需要单次检测的场景，代码更简洁。
        
        参数:
            image: OpenCV 图片 (H, W, 3)，BGR 格式
            conf_threshold: 置信度阈值
            iou_threshold: NMS IoU 阈值
            
        返回:
            list: 检测结果列表
        """
        # 预处理
        preprocess_info = self.preprocessor.letterbox(image)
        
        # 启动异步推理
        request = self.detect_async(image)
        
        # 立即获取结果
        return self.get_result(request, preprocess_info, conf_threshold, iou_threshold)
