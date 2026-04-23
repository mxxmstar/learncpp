#include <iostream>
#include <fstream>
#include <vector>
#include "decoder/ffmpeg_decoder.h"
#include "common/log/logmanager.h"

/// @brief 浠?H.264 鏂囦欢鍔犺浇娴嬭瘯鏁版嵁
std::vector<uint8_t> loadH264File(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return {};
    }
    
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    
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
    bool found_sps = false;
    bool found_pps = false;

    while (offset + 3 <= h264_data.size()) {
        bool is4byte = false;
        if (offset + 4 <= h264_data.size() &&
            h264_data[offset] == 0 &&
            h264_data[offset + 1] == 0 &&
            h264_data[offset + 2] == 0 &&
            h264_data[offset + 3] == 1) {
            is4byte = true;
            offset += 4;
        }
        else if (h264_data[offset] == 0 &&
                 h264_data[offset + 1] == 0 &&
                 h264_data[offset + 2] == 1) {
            offset += 3;
        }
        else {
            offset++;
            continue;
        }

        if (offset >= h264_data.size()) {
            break;
        }

        uint8_t nalu_type = h264_data[offset] & 0x1F;

        std::size_t next_offset = offset;
        while (next_offset + 2 < h264_data.size()) {
            if (next_offset + 3 < h264_data.size() &&
                h264_data[next_offset] == 0 &&
                h264_data[next_offset + 1] == 0 &&
                h264_data[next_offset + 2] == 0 &&
                h264_data[next_offset + 3] == 1) {
                break;
            }
            if (h264_data[next_offset] == 0 &&
                h264_data[next_offset + 1] == 0 &&
                h264_data[next_offset + 2] == 1) {
                break;
            }
            next_offset++;
        }

        size_t len = next_offset - offset;
        
        if (nalu_type == 7 && !found_sps) {
            sps.assign(h264_data.begin() + offset,
                      h264_data.begin() + offset + len);
            found_sps = true;
            std::cout << "Found SPS: " << sps.size() << " bytes" << std::endl;
        }
        else if (nalu_type == 8 && !found_pps) {
            pps.assign(h264_data.begin() + offset,
                      h264_data.begin() + offset + len);
            found_pps = true;
            std::cout << "Found PPS: " << pps.size() << " bytes" << std::endl;
        }
        
        if (found_sps && found_pps) break;
        offset = next_offset;
    }
    
    return found_sps && found_pps;
}

int main(int argc, char* argv[]) {
    std::cout << "========================================" << std::endl;
    std::cout << "FFmpegDecoder Test (Pure FFmpeg, No OpenCV)" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    try {
        // 鍒濆鍖栨棩蹇?
        LogManager& log_mgr = LogManager::getInstance();
        log_mgr.Init();
        
        std::string h264_file = "../test.h264";
        if (argc > 1) {
            h264_file = argv[1];
        }
        
        std::cout << "Testing with file: " << h264_file << std::endl;
        
        // 1. 鍔犺浇 H.264 鏂囦欢
        auto h264_data = loadH264File(h264_file);
        if (h264_data.empty()) {
            std::cerr << "No test data available." << std::endl;
            return 1;
        }
        
        // 2. 鎻愬彇 SPS/PPS
        std::vector<uint8_t> sps, pps;
        std::vector<uint8_t> extradata;
        
        if (!extractSPS_PPS(h264_data, sps, pps)) {
            std::cerr << "Failed to extract SPS/PPS" << std::endl;
            return 1;
        }
        
        // 鏋勫缓 AVCC 鏍煎紡鐨?extradata
        extradata.clear();
        extradata.push_back(1);
        extradata.push_back(sps[1]);
        extradata.push_back(sps[2]);
        extradata.push_back(sps[3]);
        extradata.push_back(0xFF);
        extradata.push_back(0xE1);
        extradata.push_back((sps.size() >> 8) & 0xFF);
        extradata.push_back(sps.size() & 0xFF);
        extradata.insert(extradata.end(), sps.begin(), sps.end());
        extradata.push_back(1);
        extradata.push_back((pps.size() >> 8) & 0xFF);
        extradata.push_back(pps.size() & 0xFF);
        extradata.insert(extradata.end(), pps.begin(), pps.end());
        
        std::cout << "Created extradata: " << extradata.size() << " bytes" << std::endl;
        
        // 3. 鍒涘缓瑙ｇ爜鍣?
        FFmpegDecoder decoder;
        decoder.setThreadCount(2);
        
        // 4. 鎵撳紑瑙ｇ爜鍣?
        bool success = decoder.open(extradata.data(), extradata.size(), 27);  // 27 = H.264
        if (!success) {
            std::cerr << "Failed to open decoder" << std::endl;
            return 1;
        }
        
        std::cout << "Decoder opened: " << decoder.getCodecName() << std::endl;
        
        // 5. 瑙ｆ瀽骞惰В鐮佹墍鏈?NALU
        int frame_count = 0;
        size_t offset = 0;
        std::vector<uint8_t> current_frame_data;
        bool first_packet_debugged = false;
        
        auto decodeCurrentFrame = [&]() {
            if (current_frame_data.empty()) return;
            
            if (!first_packet_debugged) {
                std::cout << "\n[Debug] Sending first AVCC packet (" 
                          << current_frame_data.size() << " bytes):" << std::endl;
                for (size_t i = 0; i < std::min(static_cast<size_t>(64), current_frame_data.size()); ++i) {
                    std::cout << std::hex << std::setw(2) << std::setfill('0') 
                              << static_cast<int>(current_frame_data[i]) << " ";
                    if ((i + 1) % 16 == 0) std::cout << std::endl;
                }
                std::cout << std::dec << std::endl;
                first_packet_debugged = true;
            }

            decoder.decode(current_frame_data.data(), current_frame_data.size(), frame_count * 33,
                [&frame_count](VideoFrame&& frame) {
                    if (frame.width == 0 || frame.height == 0) {
                        std::cout << "[Warning] Empty frame for pts: " << frame.pts << std::endl;
                        return;
                    }
                    
                    frame_count++;
                    if (frame_count % 30 == 0 || frame_count == 1) {
                        std::cout << "[Frame " << frame_count << "] Decoded: " 
                                  << frame.width << "x" << frame.height 
                                  << " format=" << frame.format
                                  << " @ " << frame.pts << "ms" << std::endl;
                    }
                });
            current_frame_data.clear();
        };
        
        while (offset < h264_data.size()) {
            size_t start_pos = std::string::npos;
            for (size_t i = offset; i + 2 < h264_data.size(); ++i) {
                if (h264_data[i] == 0 && h264_data[i+1] == 0 && h264_data[i+2] == 1) {
                    start_pos = i;
                    break;
                }
            }
            
            if (start_pos == std::string::npos) {
                decodeCurrentFrame();
                break;
            }
            
            size_t nalu_start = start_pos + 3;
            if (start_pos + 3 < h264_data.size() && h264_data[start_pos+3] == 1) {
                nalu_start = start_pos + 4;
            }
            
            if (nalu_start >= h264_data.size()) break;
            
            uint8_t nalu_type = h264_data[nalu_start] & 0x1F;
            
            size_t next_start = std::string::npos;
            for (size_t i = nalu_start + 1; i + 2 < h264_data.size(); ++i) {
                if (h264_data[i] == 0 && h264_data[i+1] == 0 && h264_data[i+2] == 1) {
                    next_start = i;
                    break;
                }
            }
            size_t nalu_end = (next_start != std::string::npos) ? next_start : h264_data.size();
            size_t nalu_size = nalu_end - nalu_start;
            
            bool is_slice = (nalu_type >= 1 && nalu_type <= 5);
            
            if (!is_slice) {
                offset = nalu_end;
                continue;
            }
            
            if (!current_frame_data.empty()) {
                decodeCurrentFrame();
            }
            
            current_frame_data.push_back((nalu_size >> 24) & 0xFF);
            current_frame_data.push_back((nalu_size >> 16) & 0xFF);
            current_frame_data.push_back((nalu_size >> 8) & 0xFF);
            current_frame_data.push_back(nalu_size & 0xFF);
            
            current_frame_data.insert(current_frame_data.end(), 
                                     h264_data.begin() + nalu_start, 
                                     h264_data.begin() + nalu_end);
            
            offset = nalu_end;
        }
        
        decodeCurrentFrame();
        
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

