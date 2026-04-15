"""
视频处理 gRPC 服务端（重构版）
使用算法控制器管理算法，实现 gRPC 与算法的解耦
"""

import grpc
from concurrent import futures
import time
import cv2
import numpy as np
from typing import Optional
import sys
import os

# 添加父目录到路径
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from algorithms import MockAlgorithm, YOLOV5_AVAILABLE
from algorithms import BaseAlgorithm, BoundingBox, DetectionResult

if YOLOV5_AVAILABLE:
    from algorithms import YOLOv5Algorithm

import video_processing_pb2
import video_processing_pb2_grpc

from algorithm_controller import AlgorithmController

class VideoProcessingService(video_processing_pb2_grpc.VideoProcessingServiceServicer):
    """
    视频处理 gRPC 服务
    
    职责：
    1. 处理 gRPC 通信
    2. 解码/编码视频帧
    3. 调用算法控制器处理帧
    4. 显示视频（可选）
    """
    
    def __init__(self, config: dict = None):
        """
        初始化服务
        
        Args:
            config: 配置字典
                - algorithm: 初始算法名称 ("mock" 或 "yolov5")
                - model_path: YOLOv5 模型路径
                - device: 推理设备 ("cpu" 或 "cuda")
                - show_video: 是否显示视频窗口
        """
        self.config = config or {}
        self.show_video = self.config.get('show_video', True)
        
        # 创建算法控制器
        self.controller = AlgorithmController()
        
        # 注册算法
        self._register_algorithms()
        
        # 设置初始算法
        initial_algo = self.config.get('algorithm', 'mock')
        if initial_algo in self.controller.get_available_algorithms():
            self.controller.switch_algorithm(initial_algo)
        
        print(f"[VideoService] Initialized with algorithm: {initial_algo}")
    
    def _register_algorithms(self):
        """注册所有可用算法"""
        # 注册 Mock 算法
        self.controller.register_algorithm(
            name="mock",
            algorithm_class=MockAlgorithm,
            max_boxes=3
        )
        
        # 注册 YOLOv5 算法（如果可用）
        if YOLOV5_AVAILABLE:
            model_path = self.config.get('model_path')
            device = self.config.get('device', 'cpu')
            
            if model_path:
                self.controller.register_algorithm(
                    name="yolov5",
                    algorithm_class=YOLOv5Algorithm,
                    model_path=model_path,
                    device=device
                )
            else:
                print("[VideoService] YOLOv5 available but no model path provided")
    
    def DetectObjects(self, request_iterator, context):
        """
        场景 1: 接收视频帧，返回检测结果（双向流）
        """
        print("[VideoService] DetectObjects stream started")
        
        # 创建显示窗口
        if self.show_video:
            cv2.namedWindow("Python Video Stream", cv2.WINDOW_AUTOSIZE)
        
        try:
            for frame_msg in request_iterator:
                try:
                    # 1. 解码视频帧
                    image = self._decode_frame(frame_msg)
                    if image is None:
                        continue
                    
                    # 2. 显示视频帧
                    if self.show_video:
                        cv2.imshow("Python Video Stream", image)
                        if cv2.waitKey(1) & 0xFF == ord('q'):
                            print("[VideoService] User pressed 'q', stopping...")
                            break
                    
                    # 3. 调用算法控制器处理
                    result = self.controller.process_frame(image, frame_msg.frame_id)
                    
                    if result is None:
                        continue
                    
                    # 4. 构建 gRPC 响应
                    response = self._build_detection_response(result)
                    
                    # 5. 发送响应
                    yield response
                    
                    # 打印统计信息
                    stats = self.controller.get_stats()
                    if stats['frame_count'] % 30 == 0:
                        print(f"[VideoService] {stats}")
                
                except Exception as e:
                    print(f"[VideoService] Error processing frame: {e}")
                    import traceback
                    traceback.print_exc()
                    continue
        
        finally:
            # 清理窗口
            if self.show_video:
                cv2.destroyWindow("Python Video Stream")
            
            print(f"[VideoService] DetectObjects stream ended")
    
    def ProcessAndReturnVideo(self, request_iterator, context):
        """
        场景 2: 接收视频帧，返回处理后的视频（双向流）
        """
        print("[VideoService] ProcessAndReturnVideo stream started")
        
        for frame_msg in request_iterator:
            try:
                # 1. 解码视频帧
                image = self._decode_frame(frame_msg)
                if image is None:
                    continue
                
                # 2. 调用算法检测
                result = self.controller.process_frame(image, frame_msg.frame_id)
                
                if result is None:
                    continue
                
                # 3. 绘制检测结果
                processed_image = self._draw_detections(image, result)
                
                # 4. 编码为 JPEG
                encoded_data = self._encode_frame(processed_image)
                
                # 5. 构建响应
                response = video_processing_pb2.ProcessedFrame(
                    frame_id=frame_msg.frame_id,
                    data=encoded_data,
                    width=processed_image.shape[1],
                    height=processed_image.shape[0],
                    processing_time_ms=result.processing_time_ms
                )
                
                # 6. 发送响应
                yield response
            
            except Exception as e:
                print(f"[VideoService] Error processing frame: {e}")
                import traceback
                traceback.print_exc()
                continue
        
        print("[VideoService] ProcessAndReturnVideo stream ended")
    
    def HealthCheck(self, request, context):
        """健康检查"""
        stats = self.controller.get_stats()
        print(f"[VideoService] Health check: {request.action}, Stats: {stats}")
        
        return video_processing_pb2.StreamControl(
            action=request.action,
            stream_id=request.stream_id
        )
    
    # ========== 私有方法 ==========
    
    def _decode_frame(self, frame_msg) -> Optional[np.ndarray]:
        """解码视频帧"""
        try:
            nparr = np.frombuffer(frame_msg.data, np.uint8)
            image = cv2.imdecode(nparr, cv2.IMREAD_COLOR)
            
            if image is None:
                print("[VideoService] Failed to decode frame")
                return None
            
            return image
        except Exception as e:
            print(f"[VideoService] Decode error: {e}")
            return None
    
    def _encode_frame(self, image: np.ndarray, quality: int = 85) -> bytes:
        """编码视频帧为 JPEG"""
        try:
            _, encoded = cv2.imencode('.jpg', image, [cv2.IMWRITE_JPEG_QUALITY, quality])
            return encoded.tobytes()
        except Exception as e:
            print(f"[VideoService] Encode error: {e}")
            return b""
    
    def _build_detection_response(self, result: DetectionResult):
        """构建检测结果响应"""
        return video_processing_pb2.DetectionResult(
            frame_id=result.frame_id,
            boxes=[
                video_processing_pb2.BoundingBox(
                    x=box.x,
                    y=box.y,
                    width=box.width,
                    height=box.height,
                    class_name=box.class_name,
                    confidence=box.confidence,
                    class_id=box.class_id
                )
                for box in result.boxes
            ],
            processing_time_ms=result.processing_time_ms,
            algorithm=result.algorithm,
            metadata=result.metadata
        )
    
    def _draw_detections(self, image: np.ndarray, result: DetectionResult) -> np.ndarray:
        """绘制检测结果"""
        output = image.copy()
        
        for box in result.boxes:
            x1 = int(box.x)
            y1 = int(box.y)
            x2 = x1 + int(box.width)
            y2 = y1 + int(box.height)
            
            # 绘制矩形
            cv2.rectangle(output, (x1, y1), (x2, y2), (0, 255, 0), 2)
            
            # 绘制标签
            label = f"{box.class_name}: {box.confidence:.2f}"
            cv2.putText(output, label, (x1, y1 - 10),
                       cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 2)
        
        return output
    
    def cleanup(self):
        """清理资源"""
        self.controller.cleanup()


def start_server(port: int = 50052, config: dict = None):
    """
    启动 gRPC 服务器
    
    Args:
        port: 监听端口
        config: 配置字典
    """
    # 创建服务实例
    service = VideoProcessingService(config=config)
    
    # 创建 gRPC 服务器
    server = grpc.server(
        futures.ThreadPoolExecutor(max_workers=10),
        options=[
            ('grpc.max_send_message_length', 50 * 1024 * 1024),  # 50MB
            ('grpc.max_receive_message_length', 50 * 1024 * 1024),  # 50MB
        ]
    )
    
    # 注册服务
    video_processing_pb2_grpc.add_VideoProcessingServiceServicer_to_server(service, server)
    
    # 监听端口
    ipv4_address = f'0.0.0.0:{port}'
    server.add_insecure_port(ipv4_address)
    
    # 启动服务器
    server.start()
    print(f"[VideoService] Server started on {ipv4_address}")
    print(f"[VideoService] Config: {config}")
    print("[VideoService] Ready to accept connections...")
    
    try:
        server.wait_for_termination()
    except KeyboardInterrupt:
        print("\n[VideoService] Server stopped")
        service.cleanup()
        server.stop(0)


if __name__ == '__main__':
    import argparse
    
    parser = argparse.ArgumentParser(description='Video Processing gRPC Server (Refactored)')
    parser.add_argument('--port', type=int, default=50053
                        , help='Server port')
    parser.add_argument('--algorithm', type=str, default='mock', 
                       choices=['mock', 'yolov5'], help='Algorithm to use')
    parser.add_argument('--model', type=str, default=None, help='YOLOv5 model path')
    parser.add_argument('--device', type=str, default='cpu', 
                       choices=['cpu', 'cuda'], help='Inference device')
    parser.add_argument('--no-show', action='store_true', help='Disable video display')
    
    args = parser.parse_args()
    
    config = {
        'algorithm': args.algorithm,
        'model_path': args.model,
        'device': args.device,
        'show_video': not args.no_show
    }
    
    start_server(port=args.port, config=config)
