#pragma once

#include <vector>
#include <string>
#include <map>
#include <any>
#include <cstdint>

struct DetectionResult {
    int channel_id = 0;
    int64_t timestamp = 0;

    struct BoundingBox {
        float x = 0.f, y = 0.f, width = 0.f, height = 0.f;
        float confidence = 0.f;
        int class_id = -1;
        std::string class_name;
    };

    std::vector<BoundingBox> boxes;

    struct Face {
        float x = 0.f, y = 0.f, width = 0.f, height = 0.f;
        float confidence = 0.f;
    };

    std::vector<Face> faces;
    std::map<std::string, std::any> metadata;
};