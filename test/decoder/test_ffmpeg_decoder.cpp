#include <iostream>
#include <fstream>
#include <vector>
#include "decoder/i_decoder.h"
#include "decoder/ffmpeg_decoder.h"
#include "log/logmanager.h"

/// @brief 浠?H.264 鏂囦欢鍔犺浇娴嬭瘯鏁版嵁
std::vector<uint8_t> loadH264File(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return {};
    }
    
    // 鑾峰彇鏂囦欢澶у皬
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    // 璇诲彇鏁翠釜鏂囦欢
    std::vector<uint8_t> buffer(file_size);
    file.read(reinterpret_cast<char*>(buffer.data()), file_size);
    
    std::cout << "Loaded H.264 file: " << filename 
              << " (" << file_size << " bytes)" << std::endl;
    
    return buffer;
}

/// @brief 浠?NALU 涓彁鍙?SPS/PPS
bool extractSPS_PPS(const std::vector<uint8_t>& h264_data,
                   std::vector<uint8_t>& sps,
                   std::vector<uint8_t>& pps) {
    size_t offset = 0;
    
    while (offset + 4 <= h264_data.size()) {
        // 鏌ユ壘璧峰鐮?0x00000001
        uint32_t start_code = (h264_data[offset] << 24) |
                             (h264_data[offset + 1] << 16) |
                             (h264_data[offset + 2] << 8) |
                             h264_data[offset + 3];
        
        if (start_code != 0x00000001) {
            offset++;
            continue;
        }
        
        offset += 4;  // 璺宠繃璧峰鐮?
        
        if (offset >= h264_data.size()) {
            break;
        }
        
        // 璇诲彇 NALU 绫诲瀷
        uint8_t nalu_type = h264_data[offset] & 0x1F;
        
        // 鎵惧埌涓嬩竴涓?NALU
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
        
        // 鎻愬彇 NALU 鏁版嵁
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
    std::cout << "FfmpegDecoder Test" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    try {
        // 鍒濆鍖栨棩蹇?
        LogManager& log_mgr = LogManager::getInstance();
        log_mgr.Init();
        
        // 妫€鏌ュ懡浠よ鍙傛暟
        std::string h264_file = "test.h264";
        if (argc > 1) {
            h264_file = argv[1];
        }
        
        std::cout << "Testing with file: " << h264_file << std::endl;
        
        // 1. 鍔犺浇 H.264 鏂囦欢
        auto h264_data = loadH264File(h264_file);
        if (h264_data.empty()) {
            std::cerr << "No test data available. Please provide a H.264 file." << std::endl;
            std::cout << "\nUsage: " << argv[0] << " [test.h264]" << std::endl;
            return 1;
        }
        
        // 2. 鎻愬彇 SPS/PPS
        std::vector<uint8_t> sps, pps;
        std::vector<uint8_t> extradata;
        
        if (!extractSPS_PPS(h264_data, sps, pps)) {
            std::cerr << "Failed to extract SPS/PPS from H.264 file" << std::endl;
            return 1;
        }
        
        // 鍚堝苟 SPS 鍜?PPS 浣滀负 extradata锛圓VCC 鏍煎紡锛?
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
        
        // 3. 鍒涘缓瑙ｇ爜鍣?
        FfmpegDecoder decoder;
        decoder.SetThreadCount(2);
        
        // 4. 鎵撳紑瑙ｇ爜鍣?
        bool success = decoder.Open(extradata.data(), extradata.size(), 27);  // 27 = H.264
        if (!success) {
            std::cerr << "Failed to open decoder" << std::endl;
            return 1;
        }
        
        std::cout << "Decoder opened: " << decoder.GetCodecName() << std::endl;
        
        // 5. 瑙ｆ瀽骞惰В鐮佹墍鏈?NALU
        int frame_count = 0;
        size_t offset = 0;
        
        while (offset + 4 <= h264_data.size()) {
            // 鏌ユ壘璧峰鐮?
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
            
            offset += 4;  // 璺宠繃璧峰鐮?
            
            // 鎵惧埌涓嬩竴涓?NALU
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
            
            // 璺宠繃绁炲啘 NALU锛圫PS/PPS 绛夛級
            uint8_t nalu_type = h264_data[offset] & 0x1F;
            if (nalu_type == 7 || nalu_type == 8) {
                offset = next_offset;
                continue;
            }
            
            // 解码 NALU
            size_t nalu_size = next_offset - offset;
            decoder.Decode(h264_data.data() + offset, nalu_size, frame_count * 33,
                [&frame_count](VideoFrame&& frame) {
                    frame_count++;
                    
                    if (frame_count % 30 == 0) {
                        std::cout << "[Frame " << frame_count 
                                  << "] Decoded: " << frame.width << "x" << frame.height
                                  << " @ " << frame.pts << "ms" << std::endl;
                    }
                });
            
            offset = next_offset;
        }
        
        // 绛夊緟鎵€鏈夊抚瑙ｇ爜瀹屾垚
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "Test completed!" << std::endl;
        std::cout << "Total frames decoded: " << frame_count << std::endl;
        std::cout << "Packets processed: " << decoder.GetPacketsDecoded() << std::endl;
        std::cout << "========================================" << std::endl;
        
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

