#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>
#include "common/config/common_config.h"
#include "common/log/logmanager.h"
void test_load_config() {
    std::cout << "[TEST] Loading config file..." << std::endl;
    
    auto& cm = ConfigManager::getInstance();
    bool result = cm.load("../tools/config.yaml");
    
    assert(result && "Failed to load config");
    std::cout << "  [PASS] Config loaded successfully" << std::endl;
}

void test_read_server_config() {
    std::cout << "[TEST] Reading server config..." << std::endl;
    
    const auto& config = ConfigManager::getInstance().getConfig();
    
    assert(config.server.host == "127.0.0.1" && "Wrong host");
    assert(config.server.port == 8080 && "Wrong port");
    assert(config.server.threads == 4 && "Wrong threads");
    
    std::cout << "  [PASS] Server config: host=" << config.server.host 
              << ", port=" << config.server.port 
              << ", threads=" << config.server.threads << std::endl;
}

void test_read_log_config() {
    std::cout << "[TEST] Reading log config..." << std::endl;
    
    const auto& config = ConfigManager::getInstance().getConfig();
    
    // 测试 map 格式的日志配置
    assert(config.logs.count("mainlog") > 0 && "Should have mainlog");
    assert(config.logs.count("errorlog") > 0 && "Should have errorlog");
    
    const auto& mainlog = config.logs.at("mainlog");
    assert(mainlog.level == "info" && "Wrong log level");
    assert(mainlog.dir == "./logs" && "Wrong log dir");
    assert(mainlog.max_file_size_mb == 100 && "Wrong max file size");
    assert(mainlog.rotation == "daily" && "Wrong rotation");
    
    auto level = config_utils::parseLogLevel("debug");
    assert(level == spdlog::level::debug && "Wrong level parse");
    
    std::string level_str = config_utils::logLevelToString(level);
    assert(level_str == "debug" && "Wrong level string");
    
    std::cout << "  [PASS] Log config: level=" << mainlog.level 
              << ", dir=" << mainlog.dir << std::endl;
}

void test_read_database_config() {
    std::cout << "[TEST] Reading database config..." << std::endl;
    
    const auto& config = ConfigManager::getInstance().getConfig();
    
    // 测试 camera_db 和 user_db 配置
    assert(config.camera_db.port == 3306 && "Wrong camera db port");
    assert(config.camera_db.name == "cameras" && "Wrong camera db name");
    assert(config.camera_db.pool_size == 10 && "Wrong camera db pool size");
    
    assert(config.user_db.port == 3306 && "Wrong user db port");
    assert(config.user_db.name == "users" && "Wrong user db name");
    assert(config.user_db.pool_size == 10 && "Wrong user db pool size");
    
    std::cout << "  [PASS] Database config: camera_db=" << config.camera_db.name 
              << ", user_db=" << config.user_db.name << std::endl;
}

void test_read_thread_pool_config() {
    std::cout << "[TEST] Reading thread pool config (skipped - removed from AppConfig)..." << std::endl;
    // 线程池配置已从 AppConfig 中移除
    std::cout << "  [SKIP] ThreadPool config test skipped" << std::endl;
}

void test_read_zlm_config() {
    std::cout << "[TEST] Reading zlm config..." << std::endl;
    
    const auto& config = ConfigManager::getInstance().getConfig();
    
    assert(config.zlm.zlm_host == "127.0.0.1" && "Wrong zlm host");
    assert(config.zlm.zlm_port == 8888 && "Wrong zlm port");
    assert(config.zlm.rtmp_port == 1935 && "Wrong rtmp port");
    assert(config.zlm.secret == "sphrh7r2VafHUILiTVyK3rm1C6hnUYpZ");
    
    std::cout << "  [PASS] Media config: zlm_host=" << config.zlm.zlm_host 
              << ", zlm_port=" << config.zlm.zlm_port
              << ", zlm_secret=" << config.zlm.secret << std::endl;
}

void test_validation() {
    std::cout << "[TEST] Testing validation..." << std::endl;
    
    auto& cm = ConfigManager::getInstance();
    
    assert(cm.validate() && "Default config should be valid");
    
    auto errors = cm.getValidationErrors();
    assert(errors.empty() && "Should have no errors");
    
    auto& config = cm.getConfig();
    int original_port = config.server.port;
    config.server.port = -1;
    
    errors = cm.getValidationErrors();
    assert(!errors.empty() && "Should have errors with invalid port");
    
    config.server.port = original_port;
    cm.validate();
    
    std::cout << "  [PASS] Validation working correctly" << std::endl;
}

void test_hot_reload() {
    std::cout << "[TEST] Testing hot reload..." << std::endl;
    
    auto& cm = ConfigManager::getInstance();
    cm.load("../tools/config.yaml");
    
    bool callback_called = false;
    cm.setChangeCallback([&callback_called](const AppConfig& config) {
        callback_called = true;
        std::cout << "  [CALLBACK] Config changed!" << std::endl;
    });
    
    cm.checkAndReload();
    
    std::cout << "  [PASS] Hot reload check completed" << std::endl;
}

void test_save_config() {
    std::cout << "[TEST] Testing save config..." << std::endl;
    
    auto& cm = ConfigManager::getInstance();
    cm.load("../tools/config.yaml");
    
    bool result = cm.save("../tools/config_test_save.yaml");
    assert(result && "Failed to save config");
    
    cm.load("../tools/config_test_save.yaml");
    const auto& config = cm.getConfig();
    assert(config.server.port == 8080 && "Saved config mismatch");
    
    std::filesystem::remove("../tools/config_test_save.yaml");
    
    std::cout << "  [PASS] Config saved and reloaded correctly" << std::endl;
}

int main() {
    std::cout << "=== Config Module Tests ===" << std::endl;
    LogManager& log_manager = LogManager::getInstance();
    log_manager.Init();

    test_load_config();
    test_read_server_config();
    test_read_log_config();
    test_read_database_config();
    test_read_thread_pool_config();
    test_read_zlm_config();
    test_validation();
    test_hot_reload();
    test_save_config();
    
    std::cout << std::endl << "=== All tests passed! ===" << std::endl;
    return 0;
}
