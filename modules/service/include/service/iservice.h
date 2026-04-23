#pragma once

#include <string>

/// @brief 服务接口基类
/// 所有服务都需要继承这个接口，实现统一的生命周期管理
class IService {
public:
    virtual ~IService() = default;
    
    /// @brief 服务初始化（只进行一次）
    /// @return 成功返回 true
    virtual bool Initialize() = 0;
    
    /// @brief 服务启动
    /// @return 成功返回 true
    virtual bool Start() = 0;
    
    /// @brief 服务停止
    virtual void Stop() = 0;
    
    /// @brief 获取服务名称
    virtual const char* GetName() const = 0;
    
    /// @brief 服务是否正在运行
    virtual bool IsRunning() const = 0;
    
    /// @brief 服务是否已初始化
    virtual bool IsInitialized() const = 0;
};
