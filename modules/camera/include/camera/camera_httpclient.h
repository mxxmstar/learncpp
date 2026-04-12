#pragma once
#include <string>
#include <boost/json.hpp>
#include <memory>

namespace Net {
    class PooledClient;
}

class CameraHttpClient {
public:
    explicit CameraHttpClient(const std::string& base_url, 
                             const std::string& username = "",
                             const std::string& password = "");
    
    // 通用请求
    bool Get(const std::string& api, boost::json::object& response);
    bool Post(const std::string& api, const boost::json::object& params, 
              boost::json::object& response);
    
    // 设备控制 API
    bool GetDeviceInfo(boost::json::object& info);
    bool SetVideoParams(int width, int height, int fps);
    bool SetNetworkParams(const std::string& ip, const std::string& gateway);
    bool RebootDevice();
    bool ResetFactory();
    
    // 云台控制
    bool PTZMove(int direction, int speed);
    bool PTZZoom(bool zoom_in);
    
    // 截图
    bool GetSnapshot(const std::string& save_path);
    
private:
    std::string BuildUrl(const std::string& api);
    bool ParseResponse(const boost::json::object& response, int& code, std::string& msg);
    
    std::string base_url_;
    std::string username_;
    std::string password_;
    std::shared_ptr<Net::PooledClient> http_client_;
};