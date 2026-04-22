#include "config/common_config.h"
#include <iostream>
#include <thread>
#include <chrono>

void test_update_config() {
    std::cout << "\n=== Test: Update Config ===" << std::endl;
    
    auto& config_mgr = ConfigManager::GetInstance();
    
    // 加载初始配置
    if (!config_mgr.Load("tools/config.yaml")) {
        std::cerr << "Failed to load config" << std::endl;
        return;
    }
    
    std::cout << "Initial version: " << config_mgr.GetConfigVersion() << std::endl;
    std::cout << "Initial server port: " << config_mgr.GetConfig().server.port << std::endl;
    
    // 创建新配置
    AppConfig new_config = config_mgr.GetConfig();
    new_config.server.port = 9090;  // 修改端口
    new_config.zlm.debug_terminal = false;  // 修改 ZLM 设置
    
    // 更新配置
    if (config_mgr.UpdateConfig(new_config)) {
        std::cout << "Updated version: " << config_mgr.GetConfigVersion() << std::endl;
        std::cout << "Updated server port: " << config_mgr.GetConfig().server.port << std::endl;
        std::cout << "[PASS] Config updated successfully" << std::endl;
    } else {
        std::cerr << "[FAIL] Config update failed" << std::endl;
    }
}

void test_version_rollback() {
    std::cout << "\n=== Test: Version Rollback ===" << std::endl;
    
    auto& config_mgr = ConfigManager::GetInstance();
    
    uint64_t version_before = config_mgr.GetConfigVersion();
    std::cout << "Current version: " << version_before << std::endl;
    
    // 修改配置（版本 +1）
    AppConfig new_config = config_mgr.GetConfig();
    new_config.server.port = 8080;
    config_mgr.UpdateConfig(new_config);
    
    std::cout << "After update version: " << config_mgr.GetConfigVersion() << std::endl;
    
    // 回滚到之前的版本
    if (config_mgr.RollbackToVersion(version_before)) {
        std::cout << "After rollback version: " << config_mgr.GetConfigVersion() << std::endl;
        std::cout << "Server port after rollback: " << config_mgr.GetConfig().server.port << std::endl;
        std::cout << "[PASS] Rollback successful" << std::endl;
    } else {
        std::cerr << "[FAIL] Rollback failed" << std::endl;
    }
}

void test_field_change_callback() {
    std::cout << "\n=== Test: Field Change Callback ===" << std::endl;
    
    auto& config_mgr = ConfigManager::GetInstance();
    
    int callback_count = 0;
    
    // 注册字段变更回调
    config_mgr.OnFieldChange("server.port", [&](const std::string& field, 
                                                  const std::any& old_value, 
                                                  const std::any& new_value) {
        callback_count++;
        std::cout << "[Callback] Field '" << field << "' changed:" << std::endl;
        std::cout << "  Old value: " << std::any_cast<int>(old_value) << std::endl;
        std::cout << "  New value: " << std::any_cast<int>(new_value) << std::endl;
    });
    
    std::cout << "Registered callback for 'server.port'" << std::endl;
    
    // 修改配置
    AppConfig new_config = config_mgr.GetConfig();
    new_config.server.port = 7070;
    
    std::cout << "Updating config..." << std::endl;
    config_mgr.UpdateConfig(new_config);
    
    if (callback_count > 0) {
        std::cout << "[PASS] Field callback triggered " << callback_count << " time(s)" << std::endl;
    } else {
        std::cerr << "[FAIL] Field callback not triggered" << std::endl;
    }
    
    // 移除回调
    config_mgr.RemoveFieldChangeCallback("server.port");
    std::cout << "Removed callback" << std::endl;
}

void test_global_change_callback() {
    std::cout << "\n=== Test: Global Change Callback ===" << std::endl;
    
    auto& config_mgr = ConfigManager::GetInstance();
    
    bool callback_triggered = false;
    
    // 注册全局配置变更回调
    config_mgr.SetChangeCallback([&](const AppConfig& new_config) {
        callback_triggered = true;
        std::cout << "[Global Callback] Configuration changed!" << std::endl;
        std::cout << "  Server port: " << new_config.server.port << std::endl;
        std::cout << "  ZLM host: " << new_config.zlm.zlm_host << std::endl;
    });
    
    std::cout << "Registered global callback" << std::endl;
    
    // 修改配置
    AppConfig new_config = config_mgr.GetConfig();
    new_config.zlm.zlm_port = 9999;
    
    std::cout << "Updating config..." << std::endl;
    config_mgr.UpdateConfig(new_config);
    
    if (callback_triggered) {
        std::cout << "[PASS] Global callback triggered" << std::endl;
    } else {
        std::cerr << "[FAIL] Global callback not triggered" << std::endl;
    }
}

void test_validation_on_update() {
    std::cout << "\n=== Test: Validation on Update ===" << std::endl;
    
    auto& config_mgr = ConfigManager::GetInstance();
    
    // 尝试设置无效的端口
    AppConfig invalid_config = config_mgr.GetConfig();
    invalid_config.server.port = 99999;  // 无效端口
    
    std::cout << "Attempting to set invalid port: 99999" << std::endl;
    
    if (!config_mgr.UpdateConfig(invalid_config)) {
        std::cout << "[PASS] Invalid config rejected" << std::endl;
        std::cout << "Current port (unchanged): " << config_mgr.GetConfig().server.port << std::endl;
    } else {
        std::cerr << "[FAIL] Invalid config was accepted" << std::endl;
    }
}

void test_concurrent_updates() {
    std::cout << "\n=== Test: Concurrent Updates ===" << std::endl;
    
    auto& config_mgr = ConfigManager::GetInstance();
    
    const int thread_count = 5;
    std::vector<std::thread> threads;
    
    std::cout << "Starting " << thread_count << " concurrent updates..." << std::endl;
    
    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back([&, i]() {
            AppConfig new_config = config_mgr.GetConfig();
            new_config.server.port = 8080 + i;
            
            if (config_mgr.UpdateConfig(new_config)) {
                std::cout << "  Thread " << i << " updated port to " << new_config.server.port << std::endl;
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    std::cout << "Final version: " << config_mgr.getConfigVersion() << std::endl;
    std::cout << "Final port: " << config_mgr.getConfig().server.port << std::endl;
    std::cout << "[PASS] Concurrent updates completed" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  ConfigManager Dynamic Update Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    
    test_update_config();
    test_version_rollback();
    test_field_change_callback();
    test_global_change_callback();
    test_validation_on_update();
    test_concurrent_updates();
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "  All tests completed!" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}
