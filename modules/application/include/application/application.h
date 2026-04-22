#pragma once

#include <memory>
#include <string>
#include <functional>
#include <map>
#include <vector>
#include <any>
#include "application/signal_handler.h"

// 前向声明
class IService;

/// @brief 应用程序框架
/// 提供依赖注入、配置管理、生命周期管理等功能
class Application {
public:
    using InitCallback = std::function<bool()>;
    using StartCallback = std::function<bool()>;
    using StopCallback = std::function<void()>;
    
    Application();
    ~Application();
    
    /// @brief 获取单例实例
    static Application& GetInstance();
    
    // ==================== 依赖注入容器 ====================
    
    /// @brief 注册服务（单例）
    template<typename T, typename... Args>
    void RegisterService(const std::string& name, Args&&... args) {
        services_[name] = std::make_shared<T>(std::forward<Args>(args)...);
    }
    
    /// @brief 获取服务
    template<typename T>
    std::shared_ptr<T> GetService(const std::string& name) const {
        auto it = services_.find(name);
        if (it == services_.end()) {
            return nullptr;
        }
        try {
            return std::any_cast<std::shared_ptr<T>>(it->second);
        } catch (const std::bad_any_cast&) {
            return nullptr;
        }
    }
    
    /// @brief 检查服务是否存在
    bool HasService(const std::string& name) const {
        return services_.find(name) != services_.end();
    }
    
    // ==================== 生命周期管理 ====================
    
    /// @brief 注册初始化回调
    void OnInit(InitCallback callback);
    
    /// @brief 注册启动回调
    void OnStart(StartCallback callback);
    
    /// @brief 注册停止回调
    void OnStop(StopCallback callback);
    
    /// @brief 运行应用程序（阻塞直到收到停止信号）
    int Run();
    
    /// @brief 请求停止
    void RequestStop();
    
    /// @brief 检查是否正在运行
    bool IsRunning() const { return running_.load(); }
    
    // ==================== 信号处理 ====================
    
    /// @brief 获取信号处理器
    SignalHandler& GetSignalHandler() { return signal_handler_; }
    
    // ==================== IService 管理 ====================
    
    /// @brief 注册 IService 服务（自动管理生命周期）
    template<typename T, typename... Args>
    bool RegisterService(Args&&... args) {
        try {
            auto service = std::make_shared<T>(std::forward<Args>(args)...);
            if (!service) {
                return false;
            }
            
            std::string name = service->GetName();
            
            // 检查是否已存在
            if (services_.find(name) != services_.end()) {
                return false;
            }
            
            services_[name] = service;
            service_order_.push_back(name);
            
            return true;
        } catch (...) {
            return false;
        }
    }
    
    /// @brief 获取 IService 服务
    template<typename T>
    T* GetService() {
        for (auto& [name, service] : services_) {
            auto typed_service = dynamic_cast<T*>(service.get());
            if (typed_service) {
                return typed_service;
            }
        }
        return nullptr;
    }
    
    /// @brief 通过名称获取服务
    std::shared_ptr<IService> GetService(const std::string& name) const;
    
private:
    /// @brief 执行初始化阶段
    bool initialize();
    
    /// @brief 执行启动阶段
    bool start();
    
    /// @brief 执行停止阶段
    void stop();
    
    /// @brief 优雅关闭
    void gracefulShutdown();
    
    // 生命周期回调
    std::vector<InitCallback> init_callbacks_;
    std::vector<StartCallback> start_callbacks_;
    std::vector<StopCallback> stop_callbacks_;
    
    // 状态管理
    std::atomic<bool> running_{false};
    std::atomic<bool> initialized_{false};
    
    // 信号处理器
    SignalHandler signal_handler_;
    
    // IService 服务管理
    std::map<std::string, std::shared_ptr<IService>> services_;
    std::vector<std::string> service_order_;  // 注册顺序（用于逆序停止）
};
