"""
视频处理 gRPC 服务端
支持双向流式通信，集成 YOLOv5 算法
"""

import grpc
from concurrent import futures
import time
import cv2
import numpy as np
from typing import Optional
import sys
import os

# 添加 yolov5 模块路径
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'yolov5'))

try:
    from detector import YOLOv5AsyncDetector
    YOLOV5_AVAILABLE = True
except ImportError:
    print("[Warning] YOLOv5 module not available, using mock detector")
    YOLOV5_AVAILABLE = False

import video_processing_pb2
import video_processing_pb2_grpc


class VideoProcessingService(video_processing_pb2_grpc.VideoProcessingServiceServicer):
    """视频处理服务实现"""
    
    def __init__(self, model_path: Optional[str] = None, device: str = "cpu"):
        """
        初始化服务
        
        Args:
            model_path: YOLOv5 模型路径
            device: 推理设备 ("cpu" 或 "cuda")
        """
        self.detector = None
        self.frame_count = 0
        self.total_processing_time = 0
        
        # 初始化 YOLOv5 检测器
        if YOLOV5_AVAILABLE and model_path:
            try:
                print(f"[VideoService] Loading YOLOv5 model from {model_path}")
                self.detector = YOLOv5AsyncDetector(
                    model_path=model_path,
                    device=device,
                    num_requests=4  # 4个并发请求
                )
                print("[VideoService] YOLOv5 model loaded successfully")
            except Exception as e:
                print(f"[VideoService] Failed to load YOLOv5 model: {e}")
                self.detector = None
        else:
            print("[VideoService] Using mock detector (YOLOv5 not available)")
    
    def DetectObjects(self, request_iterator, context):
        """
        场景 1: 接收视频帧，返回检测结果（双向流）
        
        Args:
            request_iterator: 视频帧流
            context: gRPC 上下文
        """
        print("[VideoService] DetectObjects stream started")
        
        for frame_msg in request_iterator:
            try:
                start_time = time.time()
                
                # 1. 解码视频帧
                image = self._decode_frame(frame_msg)
                if image is None:
                    continue
                
                # 2. 运行检测算法
                if self.detector:
                    # 使用 YOLOv5
                    detections = self._run_yolov5_detection(image)
                else:
                    # 使用模拟检测
                    detections = self._mock_detection(image)
                
                # 3. 计算处理时间
                processing_time_ms = int((time.time() - start_time) * 1000)
                self.total_processing_time += processing_time_ms
                self.frame_count += 1
                
                # 4. 构建响应
                response = video_processing_pb2.DetectionResult(
                    frame_id=frame_msg.frame_id,
                    boxes=[
                        video_processing_pb2.BoundingBox(
                            x=float(det['x']),
                            y=float(det['y']),
                            width=float(det['width']),
                            height=float(det['height']),
                            class_name=det['class_name'],
                            confidence=float(det['confidence']),
                            class_id=int(det.get('class_id', 0))
                        )
                        for det in detections
                    ],
                    processing_time_ms=processing_time_ms,
                    algorithm="YOLOv5" if self.detector else "Mock",
                    metadata={
                        "frame_count": str(self.frame_count),
                        "avg_processing_time": str(self.total_processing_time // max(self.frame_count, 1))
                    }
                )
                
                # 5. 发送响应
                yield response
                
                # 打印统计信息
                if self.frame_count % 30 == 0:
                    avg_time = self.total_processing_time // self.frame_count
                    print(f"[VideoService] Processed {self.frame_count} frames, "
                          f"Avg time: {avg_time}ms/frame")
                
            except Exception as e:
                print(f"[VideoService] Error processing frame: {e}")
                import traceback
                traceback.print_exc()
                continue
        
        print(f"[VideoService] DetectObjects stream ended. Total frames: {self.frame_count}")
    
    def ProcessAndReturnVideo(self, request_iterator, context):
        """
        场景 2: 接收视频帧，返回处理后的视频（双向流）
        
        Args:
            request_iterator: 视频帧流
            context: gRPC 上下文
        """
        print("[VideoService] ProcessAndReturnVideo stream started")
        
        for frame_msg in request_iterator:
            try:
                start_time = time.time()
                
                # 1. 解码视频帧
                image = self._decode_frame(frame_msg)
                if image is None:
                    continue
                
                # 2. 运行检测并绘制结果
                if self.detector:
                    processed_image = self._process_and_draw_yolov5(image)
                else:
                    processed_image = self._mock_process_and_draw(image)
                
                # 3. 编码为 JPEG
                encoded_data = self._encode_frame(processed_image)
                
                # 4. 计算处理时间
                processing_time_ms = int((time.time() - start_time) * 1000)
                
                # 5. 构建响应
                response = video_processing_pb2.ProcessedFrame(
                    frame_id=frame_msg.frame_id,
                    data=encoded_data,
                    width=processed_image.shape[1],
                    height=processed_image.shape[0],
                    processing_time_ms=processing_time_ms
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
        print(f"[VideoService] Health check: {request.action}")
        return video_processing_pb2.StreamControl(
            action=request.action,
            stream_id=request.stream_id
        )
    
    # ========== 私有方法 ==========
    
    def _decode_frame(self, frame_msg) -> Optional[np.ndarray]:
        """解码视频帧"""
        try:
            # 从 bytes 解码为图像
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
    
    def _run_yolov5_detection(self, image: np.ndarray) -> list:
        """运行 YOLOv5 检测"""
        try:
            # 同步检测（简化版）
            # 注意：实际使用中应该使用异步检测器
            results = []
            
            # TODO: 集成真实的 YOLOv5 检测
            # 这里返回空列表，需要实际实现
            
            return results
        except Exception as e:
            print(f"[VideoService] YOLOv5 detection error: {e}")
            return []
    
    def _process_and_draw_yolov5(self, image: np.ndarray) -> np.ndarray:
        """运行 YOLOv5 检测并绘制结果"""
        try:
            # 复制图像
            output = image.copy()
            
            # TODO: 运行检测并绘制
            # 这里只是示例
            
            return output
        except Exception as e:
            print(f"[VideoService] YOLOv5 process error: {e}")
            return image
    
    def _mock_detection(self, image: np.ndarray) -> list:
        """模拟检测（用于测试）"""
        h, w = image.shape[:2]
        
        # 生成一些随机的检测框
        import random
        num_boxes = random.randint(0, 3)
        
        detections = []
        for i in range(num_boxes):
            box_w = random.randint(50, 150)
            box_h = random.randint(50, 150)
            x = random.randint(0, w - box_w)
            y = random.randint(0, h - box_h)
            
            detections.append({
                'x': x,
                'y': y,
                'width': box_w,
                'height': box_h,
                'class_name': random.choice(['person', 'car', 'dog', 'cat']),
                'confidence': random.uniform(0.5, 0.95),
                'class_id': random.randint(0, 79)
            })
        
        return detections
    
    def _mock_process_and_draw(self, image: np.ndarray) -> np.ndarray:
        """模拟处理并绘制（用于测试）"""
        output = image.copy()
        
        # 绘制一些随机的框
        detections = self._mock_detection(image)
        
        for det in detections:
            x1 = int(det['x'])
            y1 = int(det['y'])
            x2 = x1 + int(det['width'])
            y2 = y1 + int(det['height'])
            
            # 绘制矩形
            cv2.rectangle(output, (x1, y1), (x2, y2), (0, 255, 0), 2)
            
            # 绘制标签
            label = f"{det['class_name']}: {det['confidence']:.2f}"
            cv2.putText(output, label, (x1, y1 - 10),
                       cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 2)
        
        return output


def start_server(port: int = 50052, model_path: Optional[str] = None, device: str = "cpu"):
    """
    启动 gRPC 服务器
    
    Args:
        port: 监听端口
        model_path: YOLOv5 模型路径
        device: 推理设备
    """
    # 创建服务实例
    service = VideoProcessingService(model_path=model_path, device=device)
    
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
    print(f"[VideoService] Model: {model_path if model_path else 'Mock'}")
    print(f"[VideoService] Device: {device}")
    print("[VideoService] Ready to accept connections...")
    
    try:
        server.wait_for_termination()
    except KeyboardInterrupt:
        print("\n[VideoService] Server stopped")
        server.stop(0)


if __name__ == '__main__':
    import argparse
    
    parser = argparse.ArgumentParser(description='Video Processing gRPC Server')
    parser.add_argument('--port', type=int, default=50052, help='Server port')
    parser.add_argument('--model', type=str, default=None, help='YOLOv5 model path')
    parser.add_argument('--device', type=str, default='cpu', choices=['cpu', 'cuda'],
                       help='Inference device')
    
    args = parser.parse_args()
    
    start_server(port=args.port, model_path=args.model, device=args.device)
