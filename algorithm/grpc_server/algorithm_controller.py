"""
算法控制器 - 管理算法的生命周期和切换
"""

from typing import Dict, Optional, Type
import time
from base_algorithm import BaseAlgorithm, DetectionResult


class AlgorithmController:
    """
    算法控制器
    
    职责：
    1. 管理算法的注册和初始化
    2. 支持运行时切换算法
    3. 提供统一的算法调用接口
    4. 统计算法性能
    """
    
    def __init__(self):
        self.algorithms: Dict[str, BaseAlgorithm] = {}
        self.current_algorithm: Optional[str] = None
        self.frame_count = 0
        self.total_processing_time = 0
    
    def register_algorithm(self, name: str, algorithm_class: Type[BaseAlgorithm], 
                          **kwargs) -> bool:
        """
        注册算法
        
        Args:
            name: 算法名称
            algorithm_class: 算法类
            **kwargs: 算法初始化参数
            
        Returns:
            bool: 注册成功返回 True
        """
        try:
            algorithm = algorithm_class(**kwargs)
            if algorithm.initialize():
                self.algorithms[name] = algorithm
                print(f"[AlgorithmController] Registered algorithm: {name}")
                return True
            else:
                print(f"[AlgorithmController] Failed to initialize algorithm: {name}")
                return False
        except Exception as e:
            print(f"[AlgorithmController] Error registering algorithm {name}: {e}")
            return False
    
    def switch_algorithm(self, name: str) -> bool:
        """
        切换到指定算法
        
        Args:
            name: 算法名称
            
        Returns:
            bool: 切换成功返回 True
        """
        if name not in self.algorithms:
            print(f"[AlgorithmController] Algorithm not found: {name}")
            return False
        
        # 清理当前算法
        if self.current_algorithm and self.current_algorithm in self.algorithms:
            try:
                self.algorithms[self.current_algorithm].cleanup()
            except Exception as e:
                print(f"[AlgorithmController] Error cleaning up algorithm: {e}")
        
        # 切换到新算法
        self.current_algorithm = name
        print(f"[AlgorithmController] Switched to algorithm: {name}")
        return True
    
    def process_frame(self, image, frame_id: str = "") -> Optional[DetectionResult]:
        """
        处理帧（使用当前算法）
        
        Args:
            image: 输入图像
            frame_id: 帧ID
            
        Returns:
            DetectionResult: 检测结果，失败返回 None
        """
        if not self.current_algorithm:
            print("[AlgorithmController] No algorithm selected")
            return None
        
        if self.current_algorithm not in self.algorithms:
            print(f"[AlgorithmController] Current algorithm not found: {self.current_algorithm}")
            return None
        
        algorithm = self.algorithms[self.current_algorithm]
        
        if not algorithm.is_available():
            print(f"[AlgorithmController] Algorithm not available: {self.current_algorithm}")
            return None
        
        try:
            start_time = time.time()
            
            # 调用算法处理
            result = algorithm.process(image, frame_id)
            
            # 统计
            processing_time_ms = int((time.time() - start_time) * 1000)
            result.processing_time_ms = processing_time_ms
            self.total_processing_time += processing_time_ms
            self.frame_count += 1
            
            # 添加统计信息到元数据
            result.metadata['frame_count'] = str(self.frame_count)
            result.metadata['avg_processing_time'] = str(
                self.total_processing_time // max(self.frame_count, 1)
            )
            
            return result
            
        except Exception as e:
            print(f"[AlgorithmController] Error processing frame: {e}")
            import traceback
            traceback.print_exc()
            return None
    
    def get_current_algorithm(self) -> Optional[str]:
        """获取当前算法名称"""
        return self.current_algorithm
    
    def get_available_algorithms(self) -> list:
        """获取可用算法列表"""
        return list(self.algorithms.keys())
    
    def get_stats(self) -> Dict[str, any]:
        """获取统计信息"""
        avg_time = self.total_processing_time // max(self.frame_count, 1)
        return {
            'frame_count': self.frame_count,
            'total_processing_time_ms': self.total_processing_time,
            'avg_processing_time_ms': avg_time,
            'current_algorithm': self.current_algorithm,
            'available_algorithms': self.get_available_algorithms()
        }
    
    def cleanup(self):
        """清理所有算法"""
        for name, algorithm in self.algorithms.items():
            try:
                algorithm.cleanup()
                print(f"[AlgorithmController] Cleaned up algorithm: {name}")
            except Exception as e:
                print(f"[AlgorithmController] Error cleaning up {name}: {e}")
        
        self.algorithms.clear()
        self.current_algorithm = None
        print("[AlgorithmController] All algorithms cleaned up")
