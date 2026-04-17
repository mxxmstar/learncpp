#include "camera/camera_storage.h"
#include "camera/time_utils.h"
#include "sqlite/sqlite.h"
#include "log/logmanager.h"
#include <sstream>
#include <iomanip>
#include <ctime>

// ==================== 单例实现 ====================

CameraStorage& CameraStorage::GetInstance() {
    static CameraStorage instance;
    return instance;
}

// ==================== 初始化 ====================

bool CameraStorage::Init(const std::string& db_path) {
    try {
        LOG_MAIN_INFO_AT("Initializing CameraStorage: db_path={}", db_path);
        
        // 初始化 SQLite（使用单连接，避免事务问题）
        // 原因：SQLite 连接池在多连接情况下，事务操作会因连接切换而失败
        // 详见：modules/sqlite/TRANSACTION_AND_POOL_GUIDE.md
        // Camera 设备规模不大（< 100），且已有 mutex_ 保护，单连接足够
        db_ = std::make_unique<SQLite>(db_path, 1);  // 连接池大小 1
        
        // 创建所有表
        if (!CreateTables()) {
            LOG_MAIN_ERROR_AT("Failed to create camera tables");
            return false;
        }
        
        LOG_MAIN_INFO_AT("CameraStorage initialized successfully");
        return true;
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("CameraStorage init exception: {}", e.what());
        return false;
    }
}

void CameraStorage::Shutdown() {
    LOG_MAIN_INFO_AT("Shutting down CameraStorage");
    if (db_) {
        db_->Shutdown();
        db_.reset();
    }
}

bool CameraStorage::CreateTables() {
    try {
        // 1. 创建 cameras_base 表
        {
            SQLite::SQLBuilder builder;
            builder.CreateTable("cameras_base", true)
                .Column("uuid", "TEXT", "PRIMARY KEY")
                .Column("name", "TEXT", "NOT NULL")
                .Column("vendor", "TEXT", "")
                .Column("hardware", "TEXT", "")
                .Column("software", "TEXT", "")
                .Column("serialnumber", "TEXT", "UNIQUE")
                .Column("customer", "TEXT", "")
                .Column("metadata", "TEXT", "")
                .Column("create_time", "INTEGER", "NOT NULL DEFAULT 0")  // Unix 时间戳
                .Column("update_time", "INTEGER", "NOT NULL DEFAULT 0");  // Unix 时间戳
            
            auto error = builder.Execute(*db_);
            if (error) {
                LOG_MAIN_ERROR_AT("Failed to create cameras_base table: {}", error.message);
                return false;
            }
        }
        
        // 2. 创建 cameras_connection 表
        {
            SQLite::SQLBuilder builder;
            builder.CreateTable("cameras_connection", true)
                .Column("uuid", "TEXT", "PRIMARY KEY REFERENCES cameras_base(uuid) ON DELETE CASCADE")
                .Column("rtsp_url", "TEXT", "NOT NULL")
                .Column("username", "TEXT", "")
                .Column("password", "TEXT", "");  // 前期明文存储
            
            auto error = builder.Execute(*db_);
            if (error) {
                LOG_MAIN_ERROR_AT("Failed to create cameras_connection table: {}", error.message);
                return false;
            }
        }
        
        // 3. 创建 cameras_protocol 表
        {
            SQLite::SQLBuilder builder;
            builder.CreateTable("cameras_protocol", true)
                .Column("uuid", "TEXT", "PRIMARY KEY REFERENCES cameras_base(uuid) ON DELETE CASCADE")
                .Column("protocol_type", "TEXT", "DEFAULT 'manual'")
                .Column("http_base_url", "TEXT", "")
                .Column("onvif_device_url", "TEXT", "")
                .Column("gb28181_id", "TEXT", "");
            
            auto error = builder.Execute(*db_);
            if (error) {
                LOG_MAIN_ERROR_AT("Failed to create cameras_protocol table: {}", error.message);
                return false;
            }
        }
        
        // 4. 创建 cameras_video_params 表
        {
            SQLite::SQLBuilder builder;
            builder.CreateTable("cameras_video_params", true)
                .Column("uuid", "TEXT", "PRIMARY KEY REFERENCES cameras_base(uuid) ON DELETE CASCADE")
                .Column("width", "INTEGER", "DEFAULT 1920")
                .Column("height", "INTEGER", "DEFAULT 1080")
                .Column("fps", "INTEGER", "DEFAULT 25")
                .Column("bitrate", "INTEGER", "DEFAULT 4096");
            
            auto error = builder.Execute(*db_);
            if (error) {
                LOG_MAIN_ERROR_AT("Failed to create cameras_video_params table: {}", error.message);
                return false;
            }
        }
        
        // 5. 创建 cameras_status 表
        {
            SQLite::SQLBuilder builder;
            builder.CreateTable("cameras_status", true)
                .Column("uuid", "TEXT", "PRIMARY KEY REFERENCES cameras_base(uuid) ON DELETE CASCADE")
                .Column("status", "INTEGER", "DEFAULT 0")
                .Column("last_online_time", "INTEGER", "DEFAULT 0")  // Unix 时间戳
                .Column("offline_count", "INTEGER", "DEFAULT 0")
                .Column("update_time", "INTEGER", "NOT NULL DEFAULT 0");  // Unix 时间戳
            
            auto error = builder.Execute(*db_);
            if (error) {
                LOG_MAIN_ERROR_AT("Failed to create cameras_status table: {}", error.message);
                return false;
            }
        }
        
        // 6. 创建索引
        const std::vector<std::string> indexes = {
            "CREATE INDEX IF NOT EXISTS idx_base_vendor ON cameras_base(vendor)",
            "CREATE INDEX IF NOT EXISTS idx_base_customer ON cameras_base(customer)",
            "CREATE INDEX IF NOT EXISTS idx_status_status ON cameras_status(status)"
        };
        
        for (const auto& index_sql : indexes) {
            auto err = db_->Execute(index_sql);
            if (err) {
                LOG_MAIN_WARN_AT("Failed to create index: {}", err.message);
            }
        }
        
        LOG_MAIN_INFO_AT("All camera tables created successfully");
        return true;
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("CreateTables exception: {}", e.what());
        return false;
    }
}

// ==================== CRUD 操作（完整信息）====================

bool CameraStorage::Add(const CameraInfo& camera) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // 使用事务确保原子性
        SQLite::Transaction txn(*db_);
        
        // 检查是否已存在（在事务内检查）
        std::string check_sql = "SELECT COUNT(*) FROM cameras_base WHERE uuid = ?";
        bool exists = false;
        auto error = db_->QueryWithParams(check_sql, {camera.GetUuid()},
            [&exists](void* stmt) {
                exists = (sqlite3_column_int(static_cast<sqlite3_stmt*>(stmt), 0) > 0);
            });
        
        if (error || exists) {
            LOG_MAIN_WARN_AT("Camera already exists: uuid={}", camera.GetUuid());
            txn.Rollback();
            return false;
        }
        
        int64_t now = GetCurrentTimestamp();
        
        // 1. 插入 cameras_base
        {
            std::map<std::string, std::string> values;
            values["uuid"] = camera.base.uuid;
            values["name"] = camera.base.name;
            values["vendor"] = camera.base.vendor;
            values["hardware"] = camera.base.hardware;
            values["software"] = camera.base.software;
            values["serialnumber"] = camera.base.serialnumber;
            values["customer"] = camera.base.customer;
            values["metadata"] = camera.base.metadata;
            values["create_time"] = std::to_string(now);
            values["update_time"] = std::to_string(now);
            
            auto error = db_->Insert("cameras_base", values);
            if (error) {
                LOG_MAIN_ERROR_AT("Failed to insert cameras_base: {}", error.message);
                txn.Rollback();
                return false;
            }
        }
        
        // 2. 插入 cameras_connection
        {
            std::map<std::string, std::string> values;
            values["uuid"] = camera.connection.uuid;
            values["rtsp_url"] = camera.connection.rtsp_url;
            values["username"] = camera.connection.username;
            values["password"] = camera.connection.password;  // 明文存储
            
            auto error = db_->Insert("cameras_connection", values);
            if (error) {
                LOG_MAIN_ERROR_AT("Failed to insert cameras_connection: {}", error.message);
                txn.Rollback();
                return false;
            }
        }
        
        // 3. 插入 cameras_protocol
        {
            std::map<std::string, std::string> values;
            values["uuid"] = camera.protocol.uuid;
            values["protocol_type"] = camera.protocol.protocol_type.empty() ? "manual" : camera.protocol.protocol_type;
            values["http_base_url"] = camera.protocol.http_base_url;
            values["onvif_device_url"] = camera.protocol.onvif_device_url;
            values["gb28181_id"] = camera.protocol.gb28181_id;
            
            auto error = db_->Insert("cameras_protocol", values);
            if (error) {
                LOG_MAIN_ERROR_AT("Failed to insert cameras_protocol: {}", error.message);
                txn.Rollback();
                return false;
            }
        }
        
        // 4. 插入 cameras_video_params
        {
            std::map<std::string, std::string> values;
            values["uuid"] = camera.video_params.uuid;
            values["width"] = std::to_string(camera.video_params.width);
            values["height"] = std::to_string(camera.video_params.height);
            values["fps"] = std::to_string(camera.video_params.fps);
            values["bitrate"] = std::to_string(camera.video_params.bitrate);
            
            auto error = db_->Insert("cameras_video_params", values);
            if (error) {
                LOG_MAIN_ERROR_AT("Failed to insert cameras_video_params: {}", error.message);
                txn.Rollback();
                return false;
            }
        }
        
        // 5. 插入 cameras_status
        {
            std::map<std::string, std::string> values;
            values["uuid"] = camera.status_info.uuid;
            values["status"] = std::to_string(static_cast<int>(camera.status_info.status));
            values["last_online_time"] = std::to_string(camera.status_info.last_online_time);
            values["offline_count"] = std::to_string(camera.status_info.offline_count);
            values["update_time"] = std::to_string(now);
            
            auto error = db_->Insert("cameras_status", values);
            if (error) {
                LOG_MAIN_ERROR_AT("Failed to insert cameras_status: {}", error.message);
                txn.Rollback();
                return false;
            }
        }
        
        txn.Commit();
        LOG_MAIN_INFO_AT("Camera added: uuid={}, name={}", camera.GetUuid(), camera.GetName());
        return true;
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("CameraStorage::Add exception: {}", e.what());
        return false;
    }
}

bool CameraStorage::Remove(const std::string& uuid) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // 由于外键约束 ON DELETE CASCADE，删除 base 表会自动删除其他表
        auto error = db_->Delete("cameras_base", "uuid=?", {uuid});
        if (error) {
            LOG_MAIN_WARN_AT("Camera not found or delete failed: uuid={}, error={}", uuid, error.message);
            return false;
        }
        
        LOG_MAIN_INFO_AT("Camera removed: uuid={}", uuid);
        return true;
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("CameraStorage::Remove exception: {}", e.what());
        return false;
    }
}

bool CameraStorage::Update(const CameraInfo& camera) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // 使用事务
        SQLite::Transaction txn(*db_);
        
        // 检查是否存在（在事务内检查）
        std::string check_sql = "SELECT COUNT(*) FROM cameras_base WHERE uuid = ?";
        bool exists = false;
        auto error = db_->QueryWithParams(check_sql, {camera.GetUuid()},
            [&exists](void* stmt) {
                exists = (sqlite3_column_int(static_cast<sqlite3_stmt*>(stmt), 0) > 0);
            });
        
        if (error || !exists) {
            LOG_MAIN_WARN_AT("Camera not found for update: uuid={}", camera.GetUuid());
            txn.Rollback();
            return false;
        }
        
        // 更新所有表（直接执行 SQL，不调用 Update*Info() 避免死锁）
        if (!UpdateBaseInfoInternal(camera.base) ||
            !UpdateConnectionInfoInternal(camera.connection) ||
            !UpdateProtocolInfoInternal(camera.protocol) ||
            !UpdateVideoParamsInternal(camera.video_params) ||
            !UpdateStatusInfoInternal(camera.status_info)) {
            txn.Rollback();
            return false;
        }
        
        txn.Commit();
        LOG_MAIN_INFO_AT("Camera updated: uuid={}", camera.GetUuid());
        return true;
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("CameraStorage::Update exception: {}", e.what());
        return false;
    }
}

bool CameraStorage::Get(const std::string& uuid, CameraInfo& camera) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // 使用联表查询
        std::string sql = R"(
            SELECT 
                b.uuid, b.name, b.vendor, b.hardware, b.software, b.serialnumber, 
                b.customer, b.metadata, b.create_time, b.update_time,
                c.rtsp_url, c.username, c.password,
                p.protocol_type, p.http_base_url, p.onvif_device_url, p.gb28181_id,
                v.width, v.height, v.fps, v.bitrate,
                s.status, s.last_online_time, s.offline_count, s.update_time as status_update_time
            FROM cameras_base b
            LEFT JOIN cameras_connection c ON b.uuid = c.uuid
            LEFT JOIN cameras_protocol p ON b.uuid = p.uuid
            LEFT JOIN cameras_video_params v ON b.uuid = v.uuid
            LEFT JOIN cameras_status s ON b.uuid = s.uuid
            WHERE b.uuid = ?
        )";
        
        bool found = false;
        auto error = db_->QueryWithParams(sql, {uuid}, 
            [this, &camera, &found](void* stmt) {
                auto row = SQLite::GetRowMap(stmt);
                camera = ParseCameraFromRow(row);
                found = true;
            });
        
        if (error) {
            LOG_MAIN_ERROR_AT("Failed to get camera: uuid={}, error={}", uuid, error.message);
            return false;
        }
        
        return found;
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("CameraStorage::Get exception: {}", e.what());
        return false;
    }
}

bool CameraStorage::GetAll(std::vector<CameraInfo>& cameras) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        cameras.clear();
        
        std::string sql = R"(
            SELECT 
                b.uuid, b.name, b.vendor, b.hardware, b.software, b.serialnumber, 
                b.customer, b.metadata, b.create_time, b.update_time,
                c.rtsp_url, c.username, c.password,
                p.protocol_type, p.http_base_url, p.onvif_device_url, p.gb28181_id,
                v.width, v.height, v.fps, v.bitrate,
                s.status, s.last_online_time, s.offline_count, s.update_time as status_update_time
            FROM cameras_base b
            LEFT JOIN cameras_connection c ON b.uuid = c.uuid
            LEFT JOIN cameras_protocol p ON b.uuid = p.uuid
            LEFT JOIN cameras_video_params v ON b.uuid = v.uuid
            LEFT JOIN cameras_status s ON b.uuid = s.uuid
            ORDER BY b.create_time DESC
        )";
        
        auto error = db_->Query(sql, 
            [this, &cameras](void* stmt) {
                auto row = SQLite::GetRowMap(stmt);
                cameras.push_back(ParseCameraFromRow(row));
            });
        
        if (error) {
            LOG_MAIN_ERROR_AT("Failed to get all cameras: error={}", error.message);
            return false;
        }
        
        LOG_MAIN_INFO_AT("Retrieved {} cameras", cameras.size());
        return true;
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("CameraStorage::GetAll exception: {}", e.what());
        return false;
    }
}

// ==================== 分表操作 ====================

bool CameraStorage::UpdateBaseInfo(const CameraBaseInfo& base_info) {
    try {
        std::map<std::string, std::string> values;
        values["name"] = base_info.name;
        values["vendor"] = base_info.vendor;
        values["hardware"] = base_info.hardware;
        values["software"] = base_info.software;
        values["serialnumber"] = base_info.serialnumber;
        values["customer"] = base_info.customer;
        values["metadata"] = base_info.metadata;
        values["update_time"] = std::to_string(GetCurrentTimestamp());
        
        auto error = db_->Update("cameras_base", values, "uuid=?", {base_info.uuid});
        if (error) {
            LOG_MAIN_ERROR_AT("Failed to update base info: {}", error.message);
            return false;
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool CameraStorage::UpdateConnectionInfo(const CameraConnectionInfo& conn_info) {
    try {
        std::map<std::string, std::string> values;
        values["rtsp_url"] = conn_info.rtsp_url;
        values["username"] = conn_info.username;
        values["password"] = conn_info.password;  // 明文存储
        
        auto error = db_->Update("cameras_connection", values, "uuid=?", {conn_info.uuid});
        if (error) {
            LOG_MAIN_ERROR_AT("Failed to update connection info: {}", error.message);
            return false;
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool CameraStorage::UpdateProtocolInfo(const CameraProtocolInfo& protocol_info) {
    try {
        std::map<std::string, std::string> values;
        values["protocol_type"] = protocol_info.protocol_type;
        values["http_base_url"] = protocol_info.http_base_url;
        values["onvif_device_url"] = protocol_info.onvif_device_url;
        values["gb28181_id"] = protocol_info.gb28181_id;
        
        auto error = db_->Update("cameras_protocol", values, "uuid=?", {protocol_info.uuid});
        if (error) {
            LOG_MAIN_ERROR_AT("Failed to update protocol info: {}", error.message);
            return false;
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool CameraStorage::UpdateVideoParams(const CameraVideoParams& video_params) {
    try {
        std::map<std::string, std::string> values;
        values["width"] = std::to_string(video_params.width);
        values["height"] = std::to_string(video_params.height);
        values["fps"] = std::to_string(video_params.fps);
        values["bitrate"] = std::to_string(video_params.bitrate);
        
        auto error = db_->Update("cameras_video_params", values, "uuid=?", {video_params.uuid});
        if (error) {
            LOG_MAIN_ERROR_AT("Failed to update video params: {}", error.message);
            return false;
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool CameraStorage::UpdateStatusInfo(const CameraStatusInfo& status_info) {
    try {
        std::map<std::string, std::string> values;
        values["status"] = std::to_string(static_cast<int>(status_info.status));
        values["last_online_time"] = std::to_string(status_info.last_online_time);
        values["offline_count"] = std::to_string(status_info.offline_count);
        values["update_time"] = std::to_string(GetCurrentTimestamp());
        
        auto error = db_->Update("cameras_status", values, "uuid=?", {status_info.uuid});
        if (error) {
            LOG_MAIN_ERROR_AT("Failed to update status info: {}", error.message);
            return false;
        }
        return true;
    } catch (...) {
        return false;
    }
}

// ==================== 查询操作 ====================

bool CameraStorage::GetByStatus(CameraStatus status, std::vector<CameraInfo>& cameras) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        cameras.clear();
        
        std::string sql = R"(
            SELECT 
                b.uuid, b.name, b.vendor, b.hardware, b.software, b.serialnumber, 
                b.customer, b.metadata, b.create_time, b.update_time,
                c.rtsp_url, c.username, c.password,
                p.protocol_type, p.http_base_url, p.onvif_device_url, p.gb28181_id,
                v.width, v.height, v.fps, v.bitrate,
                s.status, s.last_online_time, s.offline_count, s.update_time as status_update_time
            FROM cameras_base b
            LEFT JOIN cameras_connection c ON b.uuid = c.uuid
            LEFT JOIN cameras_protocol p ON b.uuid = p.uuid
            LEFT JOIN cameras_video_params v ON b.uuid = v.uuid
            LEFT JOIN cameras_status s ON b.uuid = s.uuid
            WHERE s.status = ?
            ORDER BY b.create_time DESC
        )";
        
        auto error = db_->QueryWithParams(sql, {std::to_string(static_cast<int>(status))},
            [this, &cameras](void* stmt) {
                auto row = SQLite::GetRowMap(stmt);
                cameras.push_back(ParseCameraFromRow(row));
            });
        
        if (error) {
            LOG_MAIN_ERROR_AT("Failed to get cameras by status: error={}", error.message);
            return false;
        }
        
        LOG_MAIN_INFO_AT("Retrieved {} cameras with status={}", 
                        cameras.size(), CameraStatusToString(status));
        return true;
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("CameraStorage::GetByStatus exception: {}", e.what());
        return false;
    }
}

bool CameraStorage::GetByVendor(const std::string& vendor, std::vector<CameraInfo>& cameras) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        cameras.clear();
        
        std::string sql = R"(
            SELECT 
                b.uuid, b.name, b.vendor, b.hardware, b.software, b.serialnumber, 
                b.customer, b.metadata, b.create_time, b.update_time,
                c.rtsp_url, c.username, c.password,
                p.protocol_type, p.http_base_url, p.onvif_device_url, p.gb28181_id,
                v.width, v.height, v.fps, v.bitrate,
                s.status, s.last_online_time, s.offline_count, s.update_time as status_update_time
            FROM cameras_base b
            LEFT JOIN cameras_connection c ON b.uuid = c.uuid
            LEFT JOIN cameras_protocol p ON b.uuid = p.uuid
            LEFT JOIN cameras_video_params v ON b.uuid = v.uuid
            LEFT JOIN cameras_status s ON b.uuid = s.uuid
            WHERE b.vendor = ?
            ORDER BY b.create_time DESC
        )";
        
        auto error = db_->QueryWithParams(sql, {vendor},
            [this, &cameras](void* stmt) {
                auto row = SQLite::GetRowMap(stmt);
                cameras.push_back(ParseCameraFromRow(row));
            });
        
        if (error) {
            LOG_MAIN_ERROR_AT("Failed to get cameras by vendor: error={}", error.message);
            return false;
        }
        
        LOG_MAIN_INFO_AT("Retrieved {} cameras from vendor={}", cameras.size(), vendor);
        return true;
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("CameraStorage::GetByVendor exception: {}", e.what());
        return false;
    }
}

// ==================== 状态管理 ====================

bool CameraStorage::UpdateStatus(const std::string& uuid, CameraStatus status) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        CameraStatusInfo status_info;
        status_info.uuid = uuid;
        status_info.status = status;
        status_info.update_time = GetCurrentTimestamp();
        
        // 如果是在线状态，更新最后在线时间
        if (status == CameraStatus::Online || status == CameraStatus::Streaming) {
            status_info.last_online_time = GetCurrentTimestamp();
        }
        
        return UpdateStatusInfo(status_info);
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("CameraStorage::UpdateStatus exception: {}", e.what());
        return false;
    }
}

// ==================== 工具方法 ====================

CameraInfo CameraStorage::ParseCameraFromRow(const std::map<std::string, std::string>& row) {
    CameraInfo camera;
    
    // 基本信息
    camera.base.uuid = GetFieldValue(row, "uuid");
    camera.base.name = GetFieldValue(row, "name");
    camera.base.vendor = GetFieldValue(row, "vendor");
    camera.base.hardware = GetFieldValue(row, "hardware");
    camera.base.software = GetFieldValue(row, "software");
    camera.base.serialnumber = GetFieldValue(row, "serialnumber");
    camera.base.customer = GetFieldValue(row, "customer");
    camera.base.metadata = GetFieldValue(row, "metadata");
    camera.base.create_time = std::stoll(GetFieldValue(row, "create_time", "0"));
    camera.base.update_time = std::stoll(GetFieldValue(row, "update_time", "0"));
    
    // 连接信息
    camera.connection.uuid = camera.base.uuid;
    camera.connection.rtsp_url = GetFieldValue(row, "rtsp_url");
    camera.connection.username = GetFieldValue(row, "username");
    camera.connection.password = GetFieldValue(row, "password");  // 明文
    
    // 协议配置
    camera.protocol.uuid = camera.base.uuid;
    camera.protocol.protocol_type = GetFieldValue(row, "protocol_type", "manual");
    camera.protocol.http_base_url = GetFieldValue(row, "http_base_url");
    camera.protocol.onvif_device_url = GetFieldValue(row, "onvif_device_url");
    camera.protocol.gb28181_id = GetFieldValue(row, "gb28181_id");
    
    // 视频参数
    camera.video_params.uuid = camera.base.uuid;
    camera.video_params.width = std::stoi(GetFieldValue(row, "width", "1920"));
    camera.video_params.height = std::stoi(GetFieldValue(row, "height", "1080"));
    camera.video_params.fps = std::stoi(GetFieldValue(row, "fps", "25"));
    camera.video_params.bitrate = std::stoi(GetFieldValue(row, "bitrate", "4096"));
    
    // 状态信息
    camera.status_info.uuid = camera.base.uuid;
    int status_int = std::stoi(GetFieldValue(row, "status", "0"));
    camera.status_info.status = static_cast<CameraStatus>(status_int);
    camera.status_info.last_online_time = std::stoll(GetFieldValue(row, "last_online_time", "0"));
    camera.status_info.offline_count = std::stoi(GetFieldValue(row, "offline_count", "0"));
    camera.status_info.update_time = std::stoll(GetFieldValue(row, "status_update_time", "0"));
    
    return camera;
}

std::string CameraStorage::GetFieldValue(const std::map<std::string, std::string>& row, 
                                         const std::string& field,
                                         const std::string& default_value) {
    auto it = row.find(field);
    if (it != row.end() && !it->second.empty()) {
        return it->second;
    }
    return default_value;
}

// ==================== 内部更新函数（不带锁，供 Update() 调用）====================

bool CameraStorage::UpdateBaseInfoInternal(const CameraBaseInfo& base_info) {
    try {
        std::map<std::string, std::string> values;
        values["name"] = base_info.name;
        values["vendor"] = base_info.vendor;
        values["hardware"] = base_info.hardware;
        values["software"] = base_info.software;
        values["serialnumber"] = base_info.serialnumber;
        values["customer"] = base_info.customer;
        values["metadata"] = base_info.metadata;
        values["update_time"] = std::to_string(GetCurrentTimestamp());
        
        auto error = db_->Update("cameras_base", values, "uuid=?", {base_info.uuid});
        if (error) {
            LOG_MAIN_ERROR_AT("Failed to update base info: {}", error.message);
            return false;
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool CameraStorage::UpdateConnectionInfoInternal(const CameraConnectionInfo& conn_info) {
    try {
        std::map<std::string, std::string> values;
        values["rtsp_url"] = conn_info.rtsp_url;
        values["username"] = conn_info.username;
        values["password"] = conn_info.password;  // 明文存储
        
        auto error = db_->Update("cameras_connection", values, "uuid=?", {conn_info.uuid});
        if (error) {
            LOG_MAIN_ERROR_AT("Failed to update connection info: {}", error.message);
            return false;
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool CameraStorage::UpdateProtocolInfoInternal(const CameraProtocolInfo& protocol_info) {
    try {
        std::map<std::string, std::string> values;
        values["protocol_type"] = protocol_info.protocol_type;
        values["http_base_url"] = protocol_info.http_base_url;
        values["onvif_device_url"] = protocol_info.onvif_device_url;
        values["gb28181_id"] = protocol_info.gb28181_id;
        
        auto error = db_->Update("cameras_protocol", values, "uuid=?", {protocol_info.uuid});
        if (error) {
            LOG_MAIN_ERROR_AT("Failed to update protocol info: {}", error.message);
            return false;
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool CameraStorage::UpdateVideoParamsInternal(const CameraVideoParams& video_params) {
    try {
        std::map<std::string, std::string> values;
        values["width"] = std::to_string(video_params.width);
        values["height"] = std::to_string(video_params.height);
        values["fps"] = std::to_string(video_params.fps);
        values["bitrate"] = std::to_string(video_params.bitrate);
        
        auto error = db_->Update("cameras_video_params", values, "uuid=?", {video_params.uuid});
        if (error) {
            LOG_MAIN_ERROR_AT("Failed to update video params: {}", error.message);
            return false;
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool CameraStorage::UpdateStatusInfoInternal(const CameraStatusInfo& status_info) {
    try {
        std::map<std::string, std::string> values;
        values["status"] = std::to_string(static_cast<int>(status_info.status));
        values["last_online_time"] = std::to_string(status_info.last_online_time);
        values["offline_count"] = std::to_string(status_info.offline_count);
        values["update_time"] = std::to_string(GetCurrentTimestamp());
        
        auto error = db_->Update("cameras_status", values, "uuid=?", {status_info.uuid});
        if (error) {
            LOG_MAIN_ERROR_AT("Failed to update status info: {}", error.message);
            return false;
        }
        return true;
    } catch (...) {
        return false;
    }
}
