"""
视频处理 gRPC 服务端模块
"""

from .video_service import VideoProcessingService, start_server

__all__ = ['VideoProcessingService', 'start_server']
