"""
YOLOv5 后处理器

提供检测结果的后处理功能：
- NMS（非极大值抑制）
- 坐标还原（从模型输出到原图坐标）
- 置信度过滤
"""

import numpy as np
from typing import List, Dict


class YOLOv5PostProcessor:
    """YOLOv5 检测结果后处理器
    
    处理模型输出，包括：
    1. 解析检测框
    2. 应用 NMS
    3. 坐标还原到原图
    4. 置信度过滤
    
    示例：
        >>> postprocessor = YOLOv5PostProcessor(conf_threshold=0.25, iou_threshold=0.45)
        >>> detections = postprocessor.process(output, preprocess_info)
    """
    
    def __init__(self, conf_threshold: float = 0.25, iou_threshold: float = 0.45):
        """初始化后处理器
        
        参数:
            conf_threshold: 置信度阈值，低于此值的检测框将被过滤
            iou_threshold: NMS 的 IoU 阈值
        """
        self.conf_threshold = conf_threshold
        self.iou_threshold = iou_threshold
    
    def process(self, output: np.ndarray, preprocess_info: dict = None) -> List[Dict]:
        """处理模型输出
        
        参数:
            output: 模型输出 (1, num_boxes, num_classes+5)
            preprocess_info: 预处理信息（包含 scale, pad_left, pad_top）
                           如果为 None，则不进行坐标还原
            
        返回:
            list: 检测结果列表，每个元素为字典
                {
                    'x1': 左上角 x 坐标,
                    'y1': 左上角 y 坐标,
                    'x2': 右下角 x 坐标,
                    'y2': 右下角 y 坐标,
                    'confidence': 置信度,
                    'class_id': 类别 ID
                }
        """
        # 解析检测框
        boxes = self._parse_boxes(output)
        
        if len(boxes) == 0:
            return []
        
        # 应用 NMS
        keep_indices = self._nms(boxes)
        
        # 构建结果
        detections = []
        for idx in keep_indices:
            box = boxes[idx]
            
            detection = {
                'x1': float(box[0]),
                'y1': float(box[1]),
                'x2': float(box[2]),
                'y2': float(box[3]),
                'confidence': float(box[4]),
                'class_id': int(box[5])
            }
            
            # 如果有预处理信息，还原坐标到原图
            if preprocess_info is not None:
                detection = self._restore_coordinates(detection, preprocess_info)
            
            detections.append(detection)
        
        return detections
    
    def _parse_boxes(self, output: np.ndarray) -> np.ndarray:
        """解析模型输出为检测框
        
        参数:
            output: 模型输出 (1, num_boxes, num_classes+5)
            
        返回:
            numpy array: 检测框数组 (num_detections, 6)
                        每行: [x1, y1, x2, y2, confidence, class_id]
        """
        # 移除 batch 维度
        output = output[0]  # (num_boxes, num_classes+5)
        
        # 提取置信度和类别
        box_confidence = output[:, 4:5]  # (num_boxes, 1)
        class_probs = output[:, 5:]  # (num_boxes, num_classes)
        
        # 计算最终置信度 = 物体置信度 * 类别概率
        final_confidence = box_confidence * class_probs  # (num_boxes, num_classes)
        
        # 获取最高置信度的类别和值
        class_ids = np.argmax(final_confidence, axis=1)  # (num_boxes,)
        max_confidence = np.max(final_confidence, axis=1)  # (num_boxes,)
        
        # 过滤低置信度的检测框
        mask = max_confidence >= self.conf_threshold
        if not np.any(mask):
            return np.array([])
        
        # 提取边界框坐标（中心点 + 宽高 -> 左上角 + 右下角）
        boxes_xywh = output[mask, :4]  # (num_filtered, 4)
        filtered_confidence = max_confidence[mask]
        filtered_class_ids = class_ids[mask]
        
        # 转换为中心点坐标
        x_center = boxes_xywh[:, 0]
        y_center = boxes_xywh[:, 1]
        width = boxes_xywh[:, 2]
        height = boxes_xywh[:, 3]
        
        # 转换为左上角 + 右下角
        x1 = x_center - width / 2
        y1 = y_center - height / 2
        x2 = x_center + width / 2
        y2 = y_center + height / 2
        
        # 组合结果
        boxes = np.column_stack([x1, y1, x2, y2, filtered_confidence, filtered_class_ids])
        
        return boxes
    
    def _nms(self, boxes: np.ndarray) -> np.ndarray:
        """非极大值抑制（NMS）
        
        参数:
            boxes: 检测框数组 (num_boxes, 6)
                  每行: [x1, y1, x2, y2, confidence, class_id]
                  
        返回:
            numpy array: 保留的检测框索引
        """
        if len(boxes) == 0:
            return np.array([])
        
        x1 = boxes[:, 0]
        y1 = boxes[:, 1]
        x2 = boxes[:, 2]
        y2 = boxes[:, 3]
        scores = boxes[:, 4]
        class_ids = boxes[:, 5]
        
        areas = (x2 - x1) * (y2 - y1)
        
        # 按置信度排序
        order = scores.argsort()[::-1]
        
        keep = []
        while order.size > 0:
            i = order[0]
            keep.append(i)
            
            # 计算 IoU
            xx1 = np.maximum(x1[i], x1[order[1:]])
            yy1 = np.maximum(y1[i], y1[order[1:]])
            xx2 = np.minimum(x2[i], x2[order[1:]])
            yy2 = np.minimum(y2[i], y2[order[1:]])
            
            w = np.maximum(0.0, xx2 - xx1)
            h = np.maximum(0.0, yy2 - yy1)
            inter = w * h
            
            # 只对同类别进行 NMS
            same_class = class_ids[order[1:]] == class_ids[i]
            iou = np.zeros_like(inter)
            iou[same_class] = inter[same_class] / (areas[i] + areas[order[1:]][same_class] - inter[same_class])
            
            # 保留 IoU 小于阈值的框
            inds = np.where(iou <= self.iou_threshold)[0]
            order = order[inds + 1]
        
        return np.array(keep, dtype=int)
    
    def _restore_coordinates(self, detection: Dict, preprocess_info: Dict) -> Dict:
        """还原坐标到原图
        
        参数:
            detection: 检测结果字典
            preprocess_info: 预处理信息
                - scale: 缩放比例
                - pad_left: 左侧填充
                - pad_top: 顶部填充
                
        返回:
            dict: 坐标还原后的检测结果
        """
        scale = preprocess_info.get('scale', 1.0)
        pad_left = preprocess_info.get('pad_left', 0)
        pad_top = preprocess_info.get('pad_top', 0)
        
        # 去除填充并还原缩放
        detection['x1'] = (detection['x1'] - pad_left) / scale
        detection['y1'] = (detection['y1'] - pad_top) / scale
        detection['x2'] = (detection['x2'] - pad_left) / scale
        detection['y2'] = (detection['y2'] - pad_top) / scale
        
        return detection
