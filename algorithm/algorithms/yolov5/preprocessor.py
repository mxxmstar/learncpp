"""
YOLOv5 图像预处理器

提供 Letterbox 图像预处理功能：
- 保持长宽比缩放
- 居中填充到固定尺寸
- 支持批量处理
"""

import cv2
import numpy as np
from typing import Tuple


class LetterboxPreprocessor:
    """Letterbox 图像预处理器
    
    保持图像长宽比，缩放到目标尺寸并填充灰色背景。
    
    工作流程：
    1. 计算缩放比例（保持长宽比）
    2. 缩放图像
    3. 创建灰色背景（114, 114, 114）
    4. 将缩放后的图像居中放置
    
    示例：
        >>> preprocessor = LetterboxPreprocessor(640, 640)
        >>> result = preprocessor(image)
        >>> print(result.image.shape)  # (640, 640, 3)
        >>> print(result.scale)        # 缩放比例
    """
    
    def __init__(self, target_width: int = 640, target_height: int = 640):
        """初始化预处理器
        
        参数:
            target_width: 目标宽度（像素）
            target_height: 目标高度（像素）
        """
        self.target_width = target_width
        self.target_height = target_height
        
    def __call__(self, img: np.ndarray) -> dict:
        """预处理图像（支持函数式调用）
        
        参数:
            img: 输入图像 (H, W, 3)，BGR 格式
            
        返回:
            dict: 包含以下字段
                - image: 预处理后的图像 (target_height, target_width, 3)
                - scale: 缩放比例
                - pad_left: 左侧填充像素数
                - pad_top: 顶部填充像素数
        """
        return self.letterbox(img)
    
    def letterbox(self, img: np.ndarray) -> dict:
        """Letterbox 图像预处理：保持长宽比缩放并填充到固定尺寸
        
        参数:
            img: 输入图像 (H, W, 3)，BGR 格式
            
        返回:
            dict: 预处理结果
                - image: 填充后的图像 (target_height, target_width, 3)
                - scale: 缩放比例
                - pad_left: 左侧填充
                - pad_top: 顶部填充
        """
        if img is None or img.size == 0:
            raise ValueError("Input image is empty or None")
        
        h, w = img.shape[:2]
        
        # 计算缩放比例（保持长宽比）
        scale = min(self.target_width / w, self.target_height / h)
        
        # 计算缩放后的尺寸
        new_w = int(w * scale)
        new_h = int(h * scale)
        
        # 缩放图像
        resized = cv2.resize(img, (new_w, new_h), interpolation=cv2.INTER_LINEAR)
        
        # 创建填充图像（使用灰色 114 填充）
        padded = np.full((self.target_height, self.target_width, 3), 114, dtype=np.uint8)
        
        # 计算填充位置（居中）
        pad_top = (self.target_height - new_h) // 2
        pad_left = (self.target_width - new_w) // 2
        
        # 将缩放后的图像复制到中心位置
        padded[pad_top:pad_top+new_h, pad_left:pad_left+new_w] = resized
        
        return {
            'image': padded,
            'scale': scale,
            'pad_left': pad_left,
            'pad_top': pad_top
        }
    
    def prepare_for_inference(self, img: np.ndarray) -> Tuple[np.ndarray, dict]:
        """准备推理输入数据
        
        完整的预处理流程：
        1. Letterbox 缩放和填充
        2. 转换为 float32 并归一化到 [0, 1]
        3. BGR -> RGB
        4. HWC -> CHW
        5. 添加 batch 维度
        
        参数:
            img: 输入图像 (H, W, 3)，BGR 格式
            
        返回:
            tuple: (推理输入数据, 预处理信息)
                - input_data: numpy array (1, 3, H, W)
                - preprocess_info: dict 包含 scale, pad_left, pad_top
        """
        # Letterbox 预处理
        result = self.letterbox(img)
        padded_img = result['image']
        
        # 转换为 float32 并归一化
        img_float = padded_img.astype(np.float32) / 255.0
        
        # BGR -> RGB
        img_rgb = img_float[:, :, ::-1]
        
        # HWC -> CHW
        img_chw = np.transpose(img_rgb, (2, 0, 1))
        
        # 添加 batch 维度
        input_data = np.expand_dims(img_chw, axis=0)
        
        # 返回预处理信息（用于坐标还原）
        preprocess_info = {
            'scale': result['scale'],
            'pad_left': result['pad_left'],
            'pad_top': result['pad_top'],
            'original_shape': img.shape[:2]  # (H, W)
        }
        
        return input_data, preprocess_info
    
    def batch_prepare(self, images: list) -> Tuple[np.ndarray, list]:
        """批量准备推理输入数据
        
        参数:
            images: 图像列表，每个元素为 (H, W, 3) BGR 格式
            
        返回:
            tuple: (批量推理输入数据, 预处理信息列表)
                - batch_data: numpy array (N, 3, H, W)
                - preprocess_infos: list 每个元素包含 scale, pad_left, pad_top
        """
        batch_data = []
        preprocess_infos = []
        for img in images:
            input_data, preprocess_info = self.prepare_for_inference(img)
            batch_data.append(input_data)
            preprocess_infos.append(preprocess_info)
        
        # 堆叠成批次
        return np.concatenate(batch_data, axis=0), preprocess_infos
