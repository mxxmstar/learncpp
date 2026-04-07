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

    // 打印前 500 个字节的十六进制
    std::cout << "\nFirst 500 bytes (hex):" << std::endl;
    size_t print_size = std::min(static_cast<size_t>(500), buffer.size());
    for (size_t i = 0; i < print_size; ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') 
                  << static_cast<int>(buffer[i]);
        
        // 每 16 个字节换行
        if ((i + 1) % 16 == 0) {
            std::cout << std::endl;
        } else if ((i + 1) % 8 == 0) {
            std::cout << "  ";  // 每 8 个字节加额外空格
        } else {
            std::cout << " ";
        }
    }
    std::cout << std::dec << std::endl;  // 恢复十进制输出
    std::cout << "Total printed: " << print_size << " bytes\n" << std::endl;
    
    return buffer;
}

/// @brief 从 NALU 中提取 SPS/PPS
bool extractSPS_PPS(const std::vector<uint8_t>& h264_data,
                   std::vector<uint8_t>& sps,
                   std::vector<uint8_t>& pps) {
    size_t offset = 0;
    bool found_sps = false;
    bool found_pps = false;

    while (offset + 3 <= h264_data.size()) {
		// 查找起始码 0x00000001 或 0x000001
        bool is4byte = false;
        if (offset + 4 <= h264_data.size() &&
            h264_data[offset] == 0 &&
            h264_data[offset + 1] == 0 &&
            h264_data[offset + 2] == 0 &&
			h264_data[offset + 3] == 1) {
            is4byte = true;
			offset += 4;  // 跳过4字节起始码
        }
        else if (h264_data[offset] == 0 &&
                 h264_data[offset + 1] == 0 &&
                 h264_data[offset + 2] == 1) {
            offset += 3;  // 跳过3字节起始码            
        }
        else {
            offset++;
            continue;
        }

        if (offset >= h264_data.size()) {
            break;
        }

        // 读取 NALU 类型
        uint8_t nalu_type = h264_data[offset] & 0x1F;

        // 找到下一个起始码
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
            // SPS
            sps.assign(h264_data.begin() + offset,
                      h264_data.begin() + offset + len);
			found_sps = true;
            std::cout << "Found SPS: " << sps.size() << " bytes" << std::endl;
        }
        else if (nalu_type == 8 && !found_pps) {
            // PPS
            pps.assign(h264_data.begin() + offset,
                      h264_data.begin() + offset + len);
            found_pps = true;
            std::cout << "Found PPS: " << pps.size() << " bytes" << std::endl;
        }
        
        // 找到就退出
        if (found_sps && found_pps) break;
        offset = next_offset;
    }
    
    return found_sps && found_pps;
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
        std::string h264_file = "../test.h264";
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
        extradata.clear();
        extradata.push_back(1);       // version
        extradata.push_back(sps[1]);  // profile
        extradata.push_back(sps[2]);  // compatibility
        extradata.push_back(sps[3]);  // level
        extradata.push_back(0xFF);    // 固定

        extradata.push_back(0xE1);    // 1 个 SPS
        extradata.push_back((sps.size() >> 8) & 0xFF);
        extradata.push_back(sps.size() & 0xFF);
        extradata.insert(extradata.end(), sps.begin(), sps.end());

        extradata.push_back(1);       // 1 个 PPS
        extradata.push_back((pps.size() >> 8) & 0xFF);
        extradata.push_back(pps.size() & 0xFF);
        extradata.insert(extradata.end(), pps.begin(), pps.end());
        
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
        
        // 临时存储当前帧的所有 NALU
        std::vector<uint8_t> current_frame_data;        
        bool first_packet_debugged = false;
        
        auto decodeCurrentFrame = [&]() {
            if (current_frame_data.empty()) return;
            
            // 【调试】打印第一个包的 Hex
            if (!first_packet_debugged) {
                std::cout << "\n[Debug] Sending first AVCC packet (" << current_frame_data.size() << " bytes):" << std::endl;
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
                    // 【关键修复】检查帧是否为空
                    if (frame.width == 0 || frame.height == 0) {
                        std::cout << "[Warning] Decoder returned an empty frame for pts: " << frame.pts << std::endl;
                        return;
                    }
                    
                    frame_count++;
                    if (frame_count % 30 == 0 || frame_count == 1) {
                        std::cout << "[Frame " << frame_count << "] Decoded successfully: " 
                                  << frame.width << "x" << frame.height 
                                  << " format: " << frame.format
                                  << " @ " << frame.pts << "ms" << std::endl;
                    }
                });
            current_frame_data.clear();
        };
        
        while (offset < h264_data.size()) {
            // 1. 查找下一个起始码
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
            
            // 2. 确定 NALU 数据的开始位置
            size_t nalu_start = start_pos + 3;
            if (start_pos + 3 < h264_data.size() && h264_data[start_pos+3] == 1) {
                nalu_start = start_pos + 4;
            }
            
            if (nalu_start >= h264_data.size()) break;
            
            // 3. 读取 NALU 类型
            uint8_t nalu_type = h264_data[nalu_start] & 0x1F;
            
            // 4. 查找下一个起始码
            size_t next_start = std::string::npos;
            for (size_t i = nalu_start + 1; i + 2 < h264_data.size(); ++i) {
                if (h264_data[i] == 0 && h264_data[i+1] == 0 && h264_data[i+2] == 1) {
                    next_start = i;
                    break;
                }
            }
            size_t nalu_end = (next_start != std::string::npos) ? next_start : h264_data.size();
            size_t nalu_size = nalu_end - nalu_start;
            
            // 5. 判断是否是新帧的开始
            bool is_slice = (nalu_type >= 1 && nalu_type <= 5);
            
            // 【关键修改】只处理切片 NALU (Type 1-5)
            // SPS(7), PPS(8), SEI(6) 等已经通过 extradata 传递给解码器了，不需要再发
            if (!is_slice) {
                offset = nalu_end;
                continue;
            }
            
            // 如果遇到切片 NALU 且当前缓存不为空，说明上一帧结束
            if (!current_frame_data.empty()) {
                decodeCurrentFrame();
            }
            
            // 6. 将当前切片 NALU 转为 AVCC 格式加入缓存
            current_frame_data.push_back((nalu_size >> 24) & 0xFF);
            current_frame_data.push_back((nalu_size >> 16) & 0xFF);
            current_frame_data.push_back((nalu_size >> 8) & 0xFF);
            current_frame_data.push_back(nalu_size & 0xFF);
            
            current_frame_data.insert(current_frame_data.end(), 
                                     h264_data.begin() + nalu_start, 
                                     h264_data.begin() + nalu_end);
            
            offset = nalu_end;
        }
        
        // 解码最后一帧
        decodeCurrentFrame();
        
        
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
