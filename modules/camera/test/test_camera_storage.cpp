#include "camera/camera_storage.h"
#include "camera/camera.h"
#include "camera/time_utils.h"
#include <iostream>
#include <cassert>
#include "common/log/logmanager.h"
void TestCameraStorage() {
    std::cout << "=== Testing CameraStorage ===" << std::endl;
    
    // 1. 初始化
    auto& storage = CameraStorage::GetInstance();
    if (!storage.Init("./test_camera.db")) {
        std::cerr << "Failed to initialize CameraStorage" << std::endl;
        return;
    }
    std::cout << " CameraStorage initialized" << std::endl;
    
    // 2. 添加摄像头
    CameraInfo camera;
    camera.base.uuid = "test_001";
    camera.base.name = "test_camera_001";
    camera.base.vendor = "hikvision";
    camera.connection.rtsp_url = "rtsp://admin:123456@192.168.1.100:554/stream";
    camera.connection.username = "admin";
    camera.connection.password = "123456";  // 明文存储
    camera.protocol.protocol_type = "manual";
    camera.video_params.width = 1920;
    camera.video_params.height = 1080;
    camera.video_params.fps = 25;
    camera.video_params.bitrate = 4096;
    
    if (!storage.Add(camera)) {
        std::cerr << "Failed to add camera" << std::endl;
        return;
    }
    std::cout << " Camera added: " << camera.base.name << std::endl;
    
    // 3. 查询摄像头
    CameraInfo retrieved;
    if (!storage.Get("test_001", retrieved)) {
        std::cerr << "Failed to get camera" << std::endl;
        return;
    }
    std::cout << "  Camera retrieved: " << retrieved.GetName() << std::endl;
    std::cout << "  - UUID: " << retrieved.GetUuid() << std::endl;
    std::cout << "  - RTSP URL: " << retrieved.GetRtspUrl() << std::endl;
    std::cout << "  - Resolution: " << retrieved.video_params.width << "x" << retrieved.video_params.height << std::endl;
    std::cout << "  - Create Time: " << TimestampToString(retrieved.base.create_time) << std::endl;
    
    // 4. 更新摄像头
    retrieved.base.name = "更新后的摄像头名称";
    retrieved.video_params.fps = 30;
    if (!storage.Update(retrieved)) {
        std::cerr << "Failed to update camera" << std::endl;
        return;
    }
    std::cout << "  Camera updated" << std::endl;
    
    // 5. 再次查询验证更新
    CameraInfo updated;
    storage.Get("test_001", updated);
    std::cout << "  - New name: " << updated.base.name << std::endl;
    std::cout << "  - New FPS: " << updated.video_params.fps << std::endl;
    
    // 6. 获取所有摄像头
    std::vector<CameraInfo> all_cameras;
    storage.GetAll(all_cameras);
    std::cout << "  Total cameras: " << all_cameras.size() << std::endl;
    
    // 7. 更新状态
    storage.UpdateStatus("test_001", CameraStatus::Online);
    CameraInfo online_camera;
    storage.Get("test_001", online_camera);
    std::cout << "  Status updated: " << CameraStatusToString(online_camera.GetStatus()) << std::endl;
    
    // 8. 按状态查询
    std::vector<CameraInfo> online_cameras;
    storage.GetByStatus(CameraStatus::Online, online_cameras);
    std::cout << "  Online cameras: " << online_cameras.size() << std::endl;
    
    // 9. 按厂商查询
    std::vector<CameraInfo> hikvision_cameras;
    storage.GetByVendor("hikvision", hikvision_cameras);
    std::cout << "  Hikvision cameras: " << hikvision_cameras.size() << std::endl;
    
    // 10. 删除摄像头
    if (!storage.Remove("test_001")) {
        std::cerr << "Failed to remove camera" << std::endl;
        return;
    }
    std::cout << "  Camera removed" << std::endl;
    
    // 验证删除
    CameraInfo deleted;
    if (storage.Get("test_001", deleted)) {
        std::cerr << "Camera should have been deleted" << std::endl;
        return;
    }
    std::cout << "  Deletion verified" << std::endl;
    
    // 清理
    storage.Shutdown();
    std::cout << "\n=== All tests passed! ===" << std::endl;
}

int main() {
    try {
        LogManager& log_manager = LogManager::getInstance();
        log_manager.Init();
        TestCameraStorage();
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
