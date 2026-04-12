#include "postprocess/osd/osd_renderer.h"
#include <sstream>
#include <iomanip>

void OsdRenderer::Render(cv::Mat& frame, 
                         int channel_id, 
                         int64_t pts,
                         float fps,
                         const std::vector<std::tuple<int, int, int, int, std::string, float>>& detection_boxes) {
    if (frame.empty()) {
        return;
    }
    
    // 1. 缁樺埗淇℃伅闈㈡澘锛堝乏涓婅锛?
    drawInfoPanel(frame, channel_id, pts, fps);
    
    // 2. 缁樺埗妫€娴嬫
    if (!detection_boxes.empty()) {
        drawDetectionBoxes(frame, detection_boxes);
    }
}

void OsdRenderer::drawInfoPanel(cv::Mat& frame, int channel_id, int64_t pts, float fps) {
    // 璁＄畻闈㈡澘灏哄
    int panel_width = 380;
    int panel_height = 90;
    int margin = 10;
    
    // 缁樺埗鍗婇€忔槑榛戣壊鑳屾櫙
    cv::Rect panel_rect(margin, margin, panel_width, panel_height);
    cv::rectangle(frame, panel_rect, cv::Scalar(0, 0, 0), -1);
    cv::rectangle(frame, panel_rect, cv::Scalar(50, 50, 50), 1);
    
    // 娣诲姞閫忔槑搴︽晥鏋?
    cv::Mat overlay;
    frame.copyTo(overlay);
    cv::rectangle(overlay, panel_rect, cv::Scalar(0, 0, 0), -1);
    cv::addWeighted(overlay, 0.5, frame, 0.5, 0, frame);
    
    // 缁樺埗鏂囨湰淇℃伅
    int font_face = cv::FONT_HERSHEY_SIMPLEX;
    double font_scale = 0.5;
    int thickness = 1;
    int line_height = 20;
    
    // 绗?1 琛岋細閫氶亾 ID
    std::string channel_text = "Channel: " + std::to_string(channel_id);
    cv::putText(frame, channel_text, 
               cv::Point(margin + 5, margin + line_height),
               font_face, font_scale, cv::Scalar(255, 255, 255), thickness);
    
    // 绗?2 琛岋細鏃堕棿鎴?
    std::string time_text = "Time: " + formatTimestamp(pts);
    cv::putText(frame, time_text, 
               cv::Point(margin + 5, margin + line_height * 2),
               font_face, font_scale, cv::Scalar(255, 255, 255), thickness);
    
    // 绗?3 琛岋細FPS锛堝甫棰滆壊锛?
    std::string fps_text = "FPS: " + std::to_string(static_cast<int>(fps));
    cv::Scalar fps_color = fps > 20 ? cv::Scalar(0, 255, 0) :   // 缁胯壊锛氭祦鐣?
                          (fps > 10 ? cv::Scalar(0, 255, 255) :  // 榛勮壊锛氫竴鑸?
                                     cv::Scalar(0, 0, 255));      // 绾㈣壊锛氬崱椤?
    cv::putText(frame, fps_text, 
               cv::Point(margin + 5, margin + line_height * 3),
               font_face, font_scale, fps_color, thickness);
    
    // 绗?4 琛岋細鍒嗚鲸鐜?
    std::string res_text = "Resolution: " + std::to_string(frame.cols) + "x" + std::to_string(frame.rows);
    cv::putText(frame, res_text, 
               cv::Point(margin + 5, margin + line_height * 4),
               font_face, font_scale, cv::Scalar(200, 200, 200), thickness);
}

void OsdRenderer::drawDetectionBoxes(cv::Mat& frame, 
                                    const std::vector<std::tuple<int, int, int, int, std::string, float>>& boxes) {
    int thickness = 2;
    int font_face = cv::FONT_HERSHEY_SIMPLEX;
    double font_scale = 0.4;
    
    for (const auto& box : boxes) {
        int x = std::get<0>(box);
        int y = std::get<1>(box);
        int w = std::get<2>(box);
        int h = std::get<3>(box);
        std::string label = std::get<4>(box);
        float confidence = std::get<5>(box);
        
        // 鏍规嵁缃俊搴﹂€夋嫨棰滆壊
        cv::Scalar color = getColorByConfidence(confidence);
        
        // 缁樺埗鐭╁舰妗?
        cv::rectangle(frame, cv::Rect(x, y, w, h), color, thickness);
        
        // 鍑嗗鏍囩鏂囨湰
        char label_buf[128];
        snprintf(label_buf, sizeof(label_buf), "%s: %.2f", label.c_str(), confidence);
        std::string label_text = label_buf;
        
        // 璁＄畻鏍囩鑳屾櫙灏哄
        int baseline = 0;
        cv::Size text_size = cv::getTextSize(label_text, font_face, font_scale, 1, &baseline);
        
        // 缁樺埗鏍囩鑳屾櫙
        cv::rectangle(frame, 
                     cv::Rect(x, y - text_size.height - 5, text_size.width + 4, text_size.height + 5),
                     color, -1);
        
        // 缁樺埗鏍囩鏂囨湰
        cv::putText(frame, label_text, 
                   cv::Point(x + 2, y - 2),
                   font_face, font_scale, cv::Scalar(255, 255, 255), 1);
    }
}

std::string OsdRenderer::formatTimestamp(int64_t pts_ms) const {
    // 灏嗘绉掕浆鎹负 HH:MM:SS.mmm 鏍煎紡
    int64_t total_seconds = pts_ms / 1000;
    int milliseconds = pts_ms % 1000;
    
    int hours = total_seconds / 3600;
    int minutes = (total_seconds % 3600) / 60;
    int seconds = total_seconds % 60;
    
    char buf[32];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%03d", hours, minutes, seconds, milliseconds);
    return std::string(buf);
}

cv::Scalar OsdRenderer::getColorByConfidence(float confidence) const {
    // 鏍规嵁缃俊搴﹁繑鍥為鑹?
    if (confidence >= 0.8f) {
        return cv::Scalar(0, 255, 0);      // 缁胯壊锛氶珮缃俊搴?
    } else if (confidence >= 0.5f) {
        return cv::Scalar(0, 255, 255);    // 榛勮壊锛氫腑绛夌疆淇″害
    } else {
        return cv::Scalar(0, 0, 255);      // 绾㈣壊锛氫綆缃俊搴?
    }
}



