"""
YOLOv5 检测器使用示例

演示如何使用同步和异步检测器进行目标检测。
"""

import cv2
import numpy as np
import time
import os
import sys

# 支持直接运行和作为模块导入两种方式
try:
    from detector import YOLOv5SyncDetector, YOLOv5AsyncDetector
    from utils import draw_detections, load_class_names
except ImportError:
    # 如果作为包的一部分运行，使用相对导入
    from .detector import YOLOv5SyncDetector, YOLOv5AsyncDetector
    from .utils import draw_detections, load_class_names


def demo_basic_usage():
    """演示 1：基本用法对比
    
    展示同步和异步推理的最简单使用方式
    """
    print("\n" + "="*70)
    print("演示 1: 基本用法对比")
    print("="*70)
    
    # 检查测试图片
    test_image = "test_image.jpg"
    if not os.path.exists(test_image):
        print(f"错误：找不到测试图片 {test_image}")
        return False
    
    image = cv2.imread(test_image)
    if image is None:
        print(f"错误：无法读取图片 {test_image}")
        return False
    
    print(f"\n测试图片: {test_image}")
    print(f"图片尺寸: {image.shape[1]}x{image.shape[0]}\n")
    
    # ========== 同步推理示例 ==========
    print("-" * 70)
    print("A. 同步推理（传统方式）")
    print("-" * 70)
    
    sync_detector = YOLOv5SyncDetector("ov_model/yolov5s.xml")
    
    print("\n执行同步推理...")
    t_start = time.time()
    output_sync = sync_detector.detect(image)
    elapsed_sync = time.time() - t_start
    
    print(f"✓ 推理完成")
    print(f"  - 耗时: {elapsed_sync*1000:.2f} ms")
    print(f"  - 检测到 {len(output_sync)} 个目标")
    print(f"  - 特点: 代码简单，但会阻塞等待\n")
    
    # ========== 异步推理示例 ==========
    print("-" * 70)
    print("B. 异步推理（高性能方式）")
    print("-" * 70)
    
    async_detector = YOLOv5AsyncDetector("ov_model/yolov5s.xml", num_requests=4)
    
    print("\n执行异步推理...")
    t_start = time.time()
    
    # 启动异步推理（立即返回）
    request = async_detector.detect_async(image)
    print("  - start_async() 已调用，推理在后台进行")
    print("  - 此时可以做其他事情（例如预处理下一张图片）")
    
    # 获取结果（如果需要的话）
    output_async = async_detector.get_result(request)
    elapsed_async = time.time() - t_start
    
    print(f"✓ 推理完成")
    print(f"  - 耗时: {elapsed_async*1000:.2f} ms")
    print(f"  - 检测到 {len(output_async)} 个目标")
    print(f"  - 特点: 可以重叠计算，提高效率\n")
    
    # 可视化结果
    class_names = load_class_names()
    result_image = draw_detections(image, output_async, class_names)
    cv2.imwrite("result.jpg", result_image)
    print(f"✓ 结果已保存到 result.jpg")
    
    return True


def demo_video_stream():
    """演示 2：视频流实时检测
    
    展示如何在视频流中使用异步检测提高 FPS
    """
    print("\n" + "="*70)
    print("演示 2: 视频流实时检测")
    print("="*70)
    
    # 打开摄像头或视频文件
    video_source = 0  # 0 表示摄像头，或替换为视频文件路径
    cap = cv2.VideoCapture(video_source)
    
    if not cap.isOpened():
        print(f"错误：无法打开视频源 {video_source}")
        return False
    
    # 创建异步检测器
    detector = YOLOv5AsyncDetector("ov_model/yolov5s.xml", num_requests=4)
    class_names = load_class_names()
    
    print("\n开始视频流检测...")
    print("按 'q' 键退出\n")
    
    frame_count = 0
    start_time = time.time()
    
    while True:
        ret, frame = cap.read()
        if not ret:
            break
        
        # 启动异步推理
        request = detector.detect_async(frame)
        
        # 在这里可以做其他事情（例如显示上一帧的结果）
        
        # 获取当前帧的检测结果
        detections = detector.get_result(request)
        
        # 绘制检测结果
        result_frame = draw_detections(frame, detections, class_names)
        
        # 计算 FPS
        frame_count += 1
        elapsed = time.time() - start_time
        fps = frame_count / elapsed if elapsed > 0 else 0
        
        # 显示 FPS
        from utils import draw_fps
        result_frame = draw_fps(result_frame, fps)
        
        # 显示结果
        cv2.imshow("YOLOv5 Detection", result_frame)
        
        # 按 'q' 退出
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break
    
    # 清理
    cap.release()
    cv2.destroyAllWindows()
    
    print(f"\n 视频处理完成")
    print(f"  - 总帧数: {frame_count}")
    print(f"  - 平均 FPS: {fps:.2f}")
    
    return True


def demo_batch_processing():
    """演示 3：批量图像处理
    
    展示如何高效处理多张图片
    """
    print("\n" + "="*70)
    print("演示 3: 批量图像处理")
    print("="*70)
    
    # 创建异步检测器
    detector = YOLOv5AsyncDetector("ov_model/yolov5s.xml", num_requests=4)
    class_names = load_class_names()
    
    # 模拟多张图片
    images = []
    for i in range(10):
        # 创建随机图像（实际使用时替换为真实图片）
        img = np.random.randint(0, 255, (480, 640, 3), dtype=np.uint8)
        images.append(img)
    
    print(f"\n处理 {len(images)} 张图片...\n")
    
    # 方法 1：逐个异步处理（推荐）
    print("方法 1: 逐个异步处理")
    t_start = time.time()
    
    requests = []
    for i, img in enumerate(images):
        request = detector.detect_async(img)
        requests.append(request)
        print(f"  - 启动图片 {i+1}/{len(images)} 的推理")
    
    # 等待所有结果
    all_detections = []
    for i, request in enumerate(requests):
        detections = detector.get_result(request)
        all_detections.append(detections)
        print(f"  - 获取图片 {i+1}/{len(images)} 的结果: {len(detections)} 个目标")
    
    elapsed = time.time() - t_start
    print(f"\n 批量处理完成")
    print(f"  - 总耗时: {elapsed*1000:.2f} ms")
    print(f"  - 平均每张: {elapsed/len(images)*1000:.2f} ms")
    
    return True


if __name__ == "__main__":
    print("\n" + "="*70)
    print("YOLOv5 OpenVINO 检测器演示")
    print("="*70)
    
    # 运行演示
    demo_basic_usage()
    
    # 取消注释以运行其他演示
    # demo_video_stream()
    # demo_batch_processing()
    
    print("\n" + "="*70)
    print("演示结束")
    print("="*70 + "\n")
