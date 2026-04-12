#pragma once

#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <ctime>
#include "camera/camera.h"

class CameraStorage {
public:
    static CameraStorage& GetInstance();
    
    bool Init(const std::string& db_path);
    void Shutdown();
    
    // CRUD 操作
    bool Add(const CameraInfo& camera);
    bool Remove(const std::string& uuid);
    bool Update(const CameraInfo& camera);
    bool Get(const std::string& uuid, CameraInfo& camera);
    bool GetAll(std::vector<CameraInfo>& cameras);
    
    // 查询操作
    bool GetByStatus(CameraStatus status, std::vector<CameraInfo>& cameras);
    bool GetByVendor(const std::string& vendor, std::vector<CameraInfo>& cameras);
    
    // 状态管理
    bool UpdateStatus(const std::string& uuid, CameraStatus status);
    
private:
    CameraStorage() = default;
    ~CameraStorage() = default;
    
    bool CreateTable();
};
