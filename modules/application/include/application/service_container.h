#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <iostream>
#include "common/service/iservice.h"
#include "log/logmanager.h"

/// @brief 服务容器
/// 负责管理所有服务的生命周期（初始化、启动、停止）
class ServiceContainer {
public:
    /// @brief 获取单例实例
    static ServiceContainer& getInstance() {
        static ServiceContainer instance;
        return instance;
    }
    
    // 禁止拷贝和赋值
    ServiceContainer(const ServiceContainer&) = delete;
    ServiceContainer& operator=(const ServiceContainer&) = delete;
    
    /// @brief 注册服务（在服务容器中创建并存储服务）
    template<typename T, typename... Args>
    bool registerService(Args&&... args) {
        try {
            auto service = std::make_shared<T>(std::forward<Args>(args)...);
            if (!service) {
                LOG_MAIN_ERROR_AT("ServiceContainer: Failed to create service");
                return false;
            }
            
            std::string name = service->GetName();
            
            // 检查是否已存在
            if (services_.find(name) != services_.end()) {
                LOG_MAIN_WARN_AT("ServiceContainer: Service '{}' already registered, skipping", name);
                return false;
            }
            
            services_[name] = service;
            service_order_.push_back(name);
            
            LOG_MAIN_INFO_AT("ServiceContainer: Registered service '{}'", name);
            return true;
            
        } catch (const std::exception& e) {
            LOG_MAIN_ERROR_AT("ServiceContainer: Exception while registering service: {}", e.what());
            return false;
        }
    }
    
    /// @brief 获取服务
    template<typename T>
    T* getService() {
        // 创建一个临时对象来获取类型名称（假设 T 有 GetName() 方法）
        // 这里我们使用一个技巧：通过 typeid 获取名称
        // 但更好的方式是让服务类提供一个静态的 getServiceName() 方法
        // 为了简单，我们遍历查找
        for (auto& [name, service] : services_) {
            auto typed_service = dynamic_cast<T*>(service.get());
            if (typed_service) {
                return typed_service;
            }
        }
        return nullptr;
    }
    
    /// @brief 获取服务（通过名称）
    std::shared_ptr<IService> getService(const std::string& name) {
        auto it = services_.find(name);
        if (it != services_.end()) {
            return it->second;
        }
        return nullptr;
    }
    
    /// @brief 初始化所有服务
    bool initializeAll() {
        LOG_MAIN_INFO_AT("ServiceContainer: Initializing all services...");
        
        for (const auto& name : service_order_) {
            auto service = services_[name];
            if (!service->IsInitialized()) {
                LOG_MAIN_INFO_AT("ServiceContainer: Initializing service '{}'", name);
                if (!service->Initialize()) {
                    LOG_MAIN_ERROR_AT("ServiceContainer: Failed to initialize service '{}'", name);
                    return false;
                }
                LOG_MAIN_INFO_AT("ServiceContainer: Service '{}' initialized successfully", name);
            }
        }
        
        LOG_MAIN_INFO_AT("ServiceContainer: All services initialized");
        return true;
    }
    
    /// @brief 启动所有服务
    bool startAll() {
        LOG_MAIN_INFO_AT("ServiceContainer: Starting all services...");
        
        for (const auto& name : service_order_) {
            auto service = services_[name];
            if (!service->IsRunning()) {
                LOG_MAIN_INFO_AT("ServiceContainer: Starting service '{}'", name);
                if (!service->Start()) {
                    LOG_MAIN_ERROR_AT("ServiceContainer: Failed to start service '{}'", name);
                    // 启动失败，停止已启动的服务
                    stopAll();
                    return false;
                }
                LOG_MAIN_INFO_AT("ServiceContainer: Service '{}' started successfully", name);
            }
        }
        
        LOG_MAIN_INFO_AT("ServiceContainer: All services started");
        return true;
    }
    
    /// @brief 停止所有服务（按注册的逆序停止）
    void stopAll() {
        LOG_MAIN_INFO_AT("ServiceContainer: Stopping all services...");
        
        // 逆序停止
        for (auto it = service_order_.rbegin(); it != service_order_.rend(); ++it) {
            const auto& name = *it;
            auto service = services_[name];
            
            if (service->IsRunning()) {
                LOG_MAIN_INFO_AT("ServiceContainer: Stopping service '{}'", name);
                service->Stop();
                LOG_MAIN_INFO_AT("ServiceContainer: Service '{}' stopped", name);
            }
        }
        
        LOG_MAIN_INFO_AT("ServiceContainer: All services stopped");
    }
    
    /// @brief 获取所有服务名称
    std::vector<std::string> getServiceNames() const {
        return service_order_;
    }
    
    /// @brief 获取服务数量
    size_t getServiceCount() const {
        return services_.size();
    }
    
private:
    ServiceContainer() = default;
    ~ServiceContainer() {
        // 析构时自动停止所有服务
        stopAll();
    }
    
    /// @brief 服务集合（key: 服务名称，value: 服务对象）
    std::unordered_map<std::string, std::shared_ptr<IService>> services_;
    
    /// @brief 服务注册顺序（用于保证启动/停止顺序）
    std::vector<std::string> service_order_;
};
