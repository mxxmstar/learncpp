#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include "video_pipeline.h"
#include "algorithm/i_algorithm.h"
#include "output/result_output.h"
#include "processor/osd_renderer.h"  // 銆愭柊澧炪€慜SD 娓叉煋鍣?
#include "log/logmanager.h"

// 鍏ㄥ眬鏍囧織
std::atomic<bool> g_running{true};

void signalHandler(int signum) {
    std::cout << "\nInterrupt signal (" << signum << ") received.\n";
    g_running = false;
}

int main() {
    // 璁剧疆淇″彿澶勭悊
    std::signal(SIGINT, signalHandler);
    
    std::cout << "========================================" << std::endl;
    std::cout << "Single Channel Video Processing Test" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    try {
        // 鍒濆鍖栨棩蹇?
        LogManager& log_mgr = LogManager::getInstance();
        log_mgr.Init();
        
        // 鍒涘缓 io_context
        boost::asio::io_context io_ctx;
        
        // 閰嶇疆娴佹按绾?
        PipelineConfig config;
        config.channel_id = 1;
        config.stream_url = "http://127.0.0.1/live/proxy_cam1.live.flv";
        config.reconnect_delay = 3;
        config.max_reconnect_attempts = -1;  // 鏃犻檺閲嶈瘯
        config.decoder_threads = 2;
        config.raw_queue_size = 64;
        config.decoded_queue_size = 16;
        config.processed_queue_size = 16;
        
        // 娣诲姞婊ら暅閾?
        config.filters = {
            "hist_eq",        // 鐩存柟鍥惧潎琛″寲
            "gaussian_blur",  // 楂樻柉妯＄硦
            "grayscale"       // 鐏板害鍖?
        };
        config.enable_preprocess = true;
        config.target_width = 640;
        config.target_height = 480;
        
        std::cout << "Pipeline Configuration:" << std::endl;
        std::cout << "  Channel ID: " << config.channel_id << std::endl;
        std::cout << "  Stream URL: " << config.stream_url << std::endl;
        std::cout << "  Filters: ";
        for (const auto& f : config.filters) {
            std::cout << f << " ";
        }
        std::cout << std::endl;
        std::cout << "  Target Size: " << config.target_width << "x" 
                  << config.target_height << std::endl;
        std::cout << "\nPress Ctrl+C to stop...\n" << std::endl;
        
        // 鍒涘缓娴佹按绾?
        VideoPipeline pipeline(io_ctx, config);
        
        // 鍒涘缓绠楁硶澶勭悊鍣?
        std::unique_ptr<IAlgorithm> algorithm;
        
        // 閫夋嫨绠楁硶绫诲瀷
        std::cout << "Select algorithm type:" << std::endl;
        std::cout << "  1. Null Algorithm (test only)" << std::endl;
        std::cout << "  2. Motion Detection" << std::endl;
        std::cout << "Default: Motion Detection" << std::endl;
        
        int algo_choice = 2;
        if (algo_choice == 1) {
            algorithm = std::make_unique<NullAlgorithm>();
        } else {
            algorithm = std::make_unique<MotionDetectionAlgorithm>();
        }
        
        std::cout << "Using algorithm: " << algorithm->GetName() << std::endl;
        
        // 鍒涘缓缁撴灉杈撳嚭鍣紙缁勫悎浣跨敤锛?
        std::vector<std::shared_ptr<IResultOutput>> outputs;
        outputs.push_back(std::make_shared<ConsoleOutput>());  // 鎺у埗鍙拌緭鍑?
        outputs.push_back(std::make_shared<LogOutput>());      // 鏃ュ織杈撳嚭
        
        // 鍙€夛細鏂囦欢杈撳嚭
        // outputs.push_back(std::make_shared<FileOutput>("results.jsonl"));
        
        // 銆愭柊澧炪€戝垱寤?OSD 娓叉煋鍣?
        OsdRenderer osd_renderer;
        
        // 璁剧疆甯ц緭鍑哄洖璋冿紙鍦?VideoPipeline 涓鐞嗙畻娉曪級
        int processed_count = 0;
        auto start_time = std::chrono::steady_clock::now();
        
        pipeline.setFrameOutputCallback(
            [&processed_count, &algorithm, &outputs, &osd_renderer, start_time](
                int channel_id, cv::Mat&& frame, int64_t pts) {
                
                processed_count++;
                
                // 璁＄畻 FPS
                auto now = std::chrono::steady_clock::now();
                double elapsed_sec = std::chrono::duration<double>(now - start_time).count();
                float fps = (elapsed_sec > 0) ? static_cast<float>(processed_count / elapsed_sec) : 0.0f;
                
                // 杩愯绠楁硶
                AlgorithmResult result = algorithm->process(frame, channel_id, pts);
                
                // 銆愭柊澧炪€戞瀯寤烘娴嬫鍒楄〃锛堢ず渚嬶細濡傛灉妫€娴嬪埌杩愬姩锛岀粯鍒朵竴涓锛?
                std::vector<std::tuple<int, int, int, int, std::string, float>> detection_boxes;
                if (result.confidence > 0.1f) {
                    // 妯℃嫙涓€涓娴嬫锛堝疄闄呭簲鐢ㄤ腑浠庣畻娉曠粨鏋滆幏鍙栵級
                    int box_x = frame.cols / 4;
                    int box_y = frame.rows / 4;
                    int box_w = frame.cols / 2;
                    int box_h = frame.rows / 2;
                    detection_boxes.emplace_back(box_x, box_y, box_w, box_h, 
                                               "Motion", result.confidence);
                }
                
                // 銆愭柊澧炪€戜娇鐢?OSD 娓叉煋鍣ㄧ粯鍒朵俊鎭?
                osd_renderer.render(frame, channel_id, pts, fps, detection_boxes);
                
                // 銆愭柊澧炪€戞樉绀虹獥鍙ｏ紙姣?3 甯ф洿鏂颁竴娆★紝鎻愰珮鎬ц兘锛?
                static int display_counter = 0;
                if (++display_counter % 3 == 0) {
                    cv::imshow("Video Processing Result - Channel " + std::to_string(channel_id), 
                              frame);
                    int key = cv::waitKey(1);  // 1ms 鍒锋柊
                    
                    // 鎸?ESC 鎴?q 閿€€鍑?
                    if (key == 27 || key == 'q' || key == 'Q') {
                        g_running = false;
                    }
                }
                
                // 杈撳嚭鍒版墍鏈夎緭鍑哄櫒
                for (auto& output : outputs) {
                    output->output(result);
                }
                
                // 姣?100 甯ф墦鍗扮粺璁?
                if (processed_count % 100 == 0) {
                    std::cout << "[Stats] Processed " << processed_count 
                              << " frames, FPS=" << fps << std::endl;
                }
            }
        );
        
        // 鍚姩娴佹按绾?
        bool success = pipeline.start();
        if (!success) {
            std::cerr << "Failed to start pipeline" << std::endl;
            return 1;
        }
        
        std::cout << "Pipeline started successfully!" << std::endl;
        
        // 鍦ㄥ悗鍙扮嚎绋嬩腑杩愯 io_context锛堝鐞嗗紓姝ョ綉缁滄搷浣滐級
        std::thread io_thread([&io_ctx]() {
            std::cout << "[IO Thread] Running io_context..." << std::endl;
            io_ctx.run();
            std::cout << "[IO Thread] io_context stopped." << std::endl;
        });
        
        // 涓诲惊鐜瓑寰?
        while (g_running && pipeline.isRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            // 鎵撳嵃缁熻淇℃伅
            uint64_t received = pipeline.getFramesReceived();
            uint64_t decoded = pipeline.getFramesDecoded();
            uint64_t processed = pipeline.getFramesProcessed();
            
            std::cout << "[Pipeline Stats] Received=" << received
                      << ", Decoded=" << decoded
                      << ", Processed=" << processed
                      << ", Algorithm=" << processed_count
                      << std::endl;
        }
        
        // 鍋滄娴佹按绾?
        std::cout << "\nStopping pipeline..." << std::endl;
        pipeline.stop();
        
        // 銆愭柊澧炪€戝叧闂墍鏈?OpenCV 绐楀彛
        cv::destroyAllWindows();
        
        // 鍋滄 io_context
        io_ctx.stop();
        
        // 绛夊緟 io 绾跨▼缁撴潫
        if (io_thread.joinable()) {
            io_thread.join();
        }
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "Test completed!" << std::endl;
        std::cout << "Total frames - Received: " << pipeline.getFramesReceived()
                  << ", Decoded: " << pipeline.getFramesDecoded()
                  << ", Processed: " << pipeline.getFramesProcessed()
                  << ", Algorithm: " << processed_count << std::endl;
        std::cout << "========================================" << std::endl;
        
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

