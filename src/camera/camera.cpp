#include "camera/camera.h"
#include <sstream>
#include <boost/json.hpp>

namespace Json = boost::json;



std::string CameraToJson(const CameraInfo& camera) {
    Json::object obj;
    obj["uuid"] = camera.uuid;
    obj["name"] = camera.name;
    obj["vendor"] = camera.vendor;
    obj["hardware"] = camera.hardware;
    obj["software"] = camera.software;
    obj["serialnumber"] = camera.serialnumber;
    obj["customer"] = camera.customer;
    obj["customer"] = camera.customer;
    obj["metadata"] = camera.metadata;
    obj["rtsp_url"] = camera.rtsp_url;
    obj["username"] = camera.username;
    obj["password"] = camera.password;
    obj["width"] = camera.width;
    obj["height"] = camera.height;
    obj["fps"] = camera.fps;
    obj["status"] = CameraStatusToString(camera.status);
    obj["created_at"] = camera.create_time;
    obj["updated_at"] = camera.update_time;
    return Json::serialize(obj);
}

std::string CameraListToJson(const std::vector<CameraInfo>& cameras) {
    Json::array arr;
    for (const auto& camera : cameras) {
        Json::object obj;
        obj["uuid"] = camera.uuid;
        obj["name"] = camera.name;
        obj["vendor"] = camera.vendor;
        obj["hardware"] = camera.hardware;
        obj["software"] = camera.software;
        obj["serialnumber"] = camera.serialnumber;
        obj["customer"] = camera.customer;
        obj["metadata"] = camera.metadata;
        obj["rtsp_url"] = camera.rtsp_url;
        obj["username"] = camera.username;
        obj["password"] = camera.password;
        obj["width"] = camera.width;
        obj["height"] = camera.height;
        obj["fps"] = camera.fps;
        obj["status"] = CameraStatusToString(camera.status);
        obj["created_at"] = camera.create_time;
        obj["updated_at"] = camera.update_time;
        arr.push_back(obj);
    }
    return Json::serialize(arr);
}
