#include <iostream>
#include <fstream>
#include <vector>
#include "video_pipeline/decoder/ffmpeg_decoder.h"
#include "log/logmanager.h"

/// @brief 从 H.264 文件加载测试数据
std::vector<uint8_t> loadH264File(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return {};
    }
    
    // 获取文件大小
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    // 读取整个文件
    std::vector<uint8_t> buffer(file_size);
    file.read(reinterpret_cast<char*>(buffer.data()), file_size);
    
    std::cout << "Loaded H.264 file: " << filename 
              << " (" << file_size << " bytes)" << std::endl;
    
    return buffer;
}

/// @brief 从 NALU 中提取 SPS/PPS
bool extractSPS_PPS(const std::vector<uint8_t>& h264_data,
                   std::vector<uint8_t>& sps,
                   std::vector<uint8_t>& pps) {
    size_t offset = 0;
    
    while (offset + 4 <= h264_data.size()) {
        // 查找起始码 0x00000001
        uint32_t start_code = (h264_data[offset] << 24) |
                             (h264_data[offset + 1] << 16) |
                             (h264_data[offset + 2] << 8) |
                             h264_data[offset + 3];
        
        if (start_code != 0x00000001) {
            offset++;
            continue;
        }
        
        offset += 4;  // 跳过起始码
        
        if (offset >= h264_data.size()) {
            break;
        }
        
        // 读取 NALU 类型
        uint8_t nalu_type = h264_data[offset] & 0x1F;
        
        // 找到下一个 NALU
        size_t next_offset = offset;
        while (next_offset + 3 < h264_data.size()) {
            if (h264_data[next_offset] == 0 &&
                h264_data[next_offset + 1] == 0 &&
                h264_data[next_offset + 2] == 0 &&
                h264_data[next_offset + 3] == 1) {
                break;
            }
            next_offset++;
        }
        
        // 提取 NALU 数据
        size_t nalu_size = next_offset - offset;
        
        if (nalu_type == 7) {
            // SPS
            sps.assign(h264_data.begin() + offset,
                      h264_data.begin() + offset + nalu_size);
            std::cout << "Found SPS: " << sps.size() << " bytes" << std::endl;
        }
        else if (nalu_type == 8) {
            // PPS
            pps.assign(h264_data.begin() + offset,
                      h264_data.begin() + offset + nalu_size);
            std::cout << "Found PPS: " << pps.size() << " bytes" << std::endl;
        }
        
        offset = next_offset;
    }
    
    return !sps.empty() && !pps.empty();
}

int main(int argc, char* argv[]) {
    std::cout << "========================================" << std::endl;
    std::cout << "FFmpegDecoder Test" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    try {
        // 初始化日志
        LogManager& log_mgr = LogManager::getInstance();
        log_mgr.Init();
        
        // 检查命令行参数
        std::string h264_file = "test.h264";
        if (argc > 1) {
            h264_file = argv[1];
        }
        
        std::cout << "Testing with file: " << h264_file << std::endl;
        
        // 1. 加载 H.264 文件
        auto h264_data = loadH264File(h264_file);
        if (h264_data.empty()) {
            std::cerr << "No test data available. Please provide a H.264 file." << std::endl;
            std::cout << "\nUsage: " << argv[0] << " [test.h264]" << std::endl;
            return 1;
        }
        
        // 2. 提取 SPS/PPS
        std::vector<uint8_t> sps, pps;
        std::vector<uint8_t> extradata;
        
        if (!extractSPS_PPS(h264_data, sps, pps)) {
            std::cerr << "Failed to extract SPS/PPS from H.264 file" << std::endl;
            return 1;
        }
        
        // 合并 SPS 和 PPS 作为 extradata
        // AVCC 格式：[SPS length][SPS][PPS length][PPS]
        extradata.resize(5 + sps.size() + 3 + pps.size());
        size_t pos = 0;
        
        // SPS
        extradata[pos++] = 1;  // version
        extradata[pos++] = sps[1];  // profile
        extradata[pos++] = sps[2];  // compatibility
        extradata[pos++] = sps[3];  // level
        extradata[pos++] = 0xff;  // 6 bits reserved + 2 bits nal length size - 1
        
        extradata[pos++] = 0xe1;  // 3 bits reserved + 5 bits num SPS
        extradata[pos++] = (sps.size() >> 8) & 0xff;
        extradata[pos++] = sps.size() & 0xff;
        memcpy(extradata.data() + pos, sps.data(), sps.size());
        pos += sps.size();
        
        // PPS
        extradata[pos++] = 1;  // num PPS
        extradata[pos++] = (pps.size() >> 8) & 0xff;
        extradata[pos++] = pps.size() & 0xff;
        memcpy(extradata.data() + pos, pps.data(), pps.size());
        
        std::cout << "Created extradata: " << extradata.size() << " bytes" << std::endl;
        
        // 3. 创建解码器
        FFmpegDecoder decoder;
        decoder.setThreadCount(2);
        
        // 4. 打开解码器
        bool success = decoder.open(extradata.data(), extradata.size(), 27);  // 27 = H.264
        if (!success) {
            std::cerr << "Failed to open decoder" << std::endl;
            return 1;
        }
        
        std::cout << "Decoder opened: " << decoder.getCodecName() << std::endl;
        
        // 5. 解析并解码所有 NALU
        int frame_count = 0;
        size_t offset = 0;
        
        while (offset + 4 <= h264_data.size()) {
            // 查找起始码
            while (offset + 3 < h264_data.size() &&
                  (h264_data[offset] != 0 ||
                   h264_data[offset + 1] != 0 ||
                   h264_data[offset + 2] != 0 ||
                   h264_data[offset + 3] != 1)) {
                offset++;
            }
            
            if (offset + 3 >= h264_data.size()) {
                break;
            }
            
            offset += 4;  // 跳过起始码
            
            // 找到下一个 NALU
            size_t next_offset = offset;
            while (next_offset + 3 < h264_data.size()) {
                if (h264_data[next_offset] == 0 &&
                    h264_data[next_offset + 1] == 0 &&
                    h264_data[next_offset + 2] == 0 &&
                    h264_data[next_offset + 3] == 1) {
                    break;
                }
                next_offset++;
            }
            
            // 跳过神农 NALU（SPS/PPS 等）
            uint8_t nalu_type = h264_data[offset] & 0x1F;
            if (nalu_type == 7 || nalu_type == 8) {
                offset = next_offset;
                continue;
            }
            
            // 解码 NALU
            size_t nalu_size = next_offset - offset;
            decoder.decode(h264_data.data() + offset, nalu_size, frame_count * 33,
                [&frame_count](cv::Mat&& frame, int64_t pts) {
                    frame_count++;
                    
                    if (frame_count % 30 == 0) {
                        std::cout << "[Frame " << frame_count 
                                  << "] Decoded: " << frame.cols << "x" << frame.rows
                                  << " @ " << pts << "ms" << std::endl;
                    }
                });
            
            offset = next_offset;
        }
        
        // 等待所有帧解码完成
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "Test completed!" << std::endl;
        std::cout << "Total frames decoded: " << frame_count << std::endl;
        std::cout << "Packets processed: " << decoder.getPacketsDecoded() << std::endl;
        std::cout << "========================================" << std::endl;
        
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
