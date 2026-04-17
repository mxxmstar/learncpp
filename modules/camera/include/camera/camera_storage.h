#pragma once

#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <ctime>
#include <string>
#include <map>  // 需要 std::map
#include "camera/camera.h"

class SQLite;  // 前向声明

class CameraStorage {
public:
    static CameraStorage& GetInstance();
    
    bool Init(const std::string& db_path);
    void Shutdown();
    
    // ==================== CRUD 操作（完整信息）====================
    bool Add(const CameraInfo& camera);           // 添加摄像头（所有表）
    bool Remove(const std::string& uuid);         // 删除摄像头（所有表）
    bool Update(const CameraInfo& camera);        // 更新摄像头（所有表）
    bool Get(const std::string& uuid, CameraInfo& camera);  // 查询单个（联表查询）
    bool GetAll(std::vector<CameraInfo>& cameras);  // 查询全部
    
    // ==================== 分表操作（单独更新某部分）====================
    bool UpdateBaseInfo(const CameraBaseInfo& base_info);
    bool UpdateConnectionInfo(const CameraConnectionInfo& conn_info);
    bool UpdateProtocolInfo(const CameraProtocolInfo& protocol_info);
    bool UpdateVideoParams(const CameraVideoParams& video_params);
    bool UpdateStatusInfo(const CameraStatusInfo& status_info);
    
    // ==================== 查询操作 ====================
    bool GetByStatus(CameraStatus status, std::vector<CameraInfo>& cameras);
    bool GetByVendor(const std::string& vendor, std::vector<CameraInfo>& cameras);
    
    // ==================== 状态管理 ====================
    bool UpdateStatus(const std::string& uuid, CameraStatus status);
    
private:
    CameraStorage() = default;
    ~CameraStorage() = default;
    
    bool CreateTables();  // 创建所有表
    
    // 工具方法
    CameraInfo ParseCameraFromRow(const std::map<std::string, std::string>& row);
    std::string GetFieldValue(const std::map<std::string, std::string>& row, 
                             const std::string& field,
                             const std::string& default_value = "");
    
    // 内部更新函数（不带锁，供 Update() 调用）
    bool UpdateBaseInfoInternal(const CameraBaseInfo& base_info);
    bool UpdateConnectionInfoInternal(const CameraConnectionInfo& conn_info);
    bool UpdateProtocolInfoInternal(const CameraProtocolInfo& protocol_info);
    bool UpdateVideoParamsInternal(const CameraVideoParams& video_params);
    bool UpdateStatusInfoInternal(const CameraStatusInfo& status_info);
    
    std::unique_ptr<SQLite> db_;  // SQLite 数据库实例
    mutable std::mutex mutex_;    // 线程安全锁
};
