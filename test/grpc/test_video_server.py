"""
视频处理 gRPC 服务端测试
"""

import sys
import os
import time
import cv2
import numpy as np

# 添加 grpc_server 模块路径
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'algorithm', 'grpc_server'))

from video_service import start_server


def create_test_video(output_path: str = "test_video.mp4", num_frames: int = 100):
    """创建测试视频"""
    print(f"Creating test video with {num_frames} frames...")
    
    fourcc = cv2.VideoWriter_fourcc(*'mp4v')
    out = cv2.VideoWriter(output_path, fourcc, 30.0, (640, 480))
    
    for i in range(num_frames):
        # 创建彩色帧
        frame = np.zeros((480, 640, 3), dtype=np.uint8)
        
        # 绘制移动的矩形
        x = int((i * 5) % 640)
        y = int((i * 3) % 480)
        cv2.rectangle(frame, (x, y), (x + 50, y + 50), (0, 255, 0), -1)
        
        # 添加帧号
        cv2.putText(frame, f"Frame {i}", (10, 30),
                   cv2.FONT_HERSHEY_SIMPLEX, 1, (255, 255, 255), 2)
        
        out.write(frame)
    
    out.release()
    print(f"Test video saved to: {output_path}")
    return output_path


def main():
    """启动测试服务器"""
    print("="*60)
    print("Video Processing gRPC Server Test")
    print("="*60)
    
    # 创建测试视频
    test_video = create_test_video()
    
    print("\nStarting gRPC server on port 50052...")
    print("Press Ctrl+C to stop\n")
    
    try:
        # 启动服务器（使用模拟检测器）
        start_server(port=50052, model_path=None, device="cpu")
    except KeyboardInterrupt:
        print("\nServer stopped by user")
    except Exception as e:
        print(f"\nServer error: {e}")
        import traceback
        traceback.print_exc()


if __name__ == '__main__':
    main()
