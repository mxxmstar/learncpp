#include "alg/inference/openvino_cpu_engine.h"
#include "common/log/logmanager.h"
#include <cstring>

OpenVinoCpuEngine::OpenVinoCpuEngine() {
    start_time_ = std::chrono::steady_clock::now();
}

OpenVinoCpuEngine::~OpenVinoCpuEngine() {
    if (running_) {
        running_ = false;
        queue_cv_.notify_all();
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }
}

bool OpenVinoCpuEngine::LoadModel(const InferenceConfig& config) {
    try {
        LOG_MAIN_INFO_AT("LoadModel started");
        LOG_MAIN_INFO_AT("Model path: {}", config.model_path);
        LOG_MAIN_INFO_AT("Device: {}", config.device);
        LOG_MAIN_INFO_AT("Async mode: {}", (config.async_mode ? "true" : "false"));
        
        config_ = config;
        async_mode_ = config.async_mode;
        
        // 1. 读取模型
        LOG_MAIN_DEBUG_AT("Loading OpenVINO model from: {}", config.model_path);
        LOG_MAIN_DEBUG_AT("Calling core_.read_model...");
        auto model_ptr = core_.read_model(config.model_path);
        LOG_MAIN_INFO_AT("Model read successfully");
        
        // 2. 配置 PrePostProcessor（如果启用，在编译之前应用）
        if (config.enable_preprocessor) {
            LOG_MAIN_INFO_AT("Configuring PrePostProcessor before compilation...");
            preprocessor_ = std::make_unique<PrePostProcessor>();
            
            auto processed_model = preprocessor_->Configure(model_ptr, config.preprocess_config);
            if (processed_model) {
                use_preprocessor_ = true;
                model_ptr = processed_model;  // 使用处理后的模型
                LOG_MAIN_INFO_AT("PrePostProcessor configured successfully");
            } else {
                LOG_MAIN_INFO_AT("Failed to configure PrePostProcessor, falling back to manual preprocessing"); 
                use_preprocessor_ = false;
                preprocessor_.reset();
            }
        }
        
        // 3. 编译模型
        std::string device = config.device.empty() ? "CPU" : config.device;
        LOG_MAIN_INFO_AT("Compiling model for device: {}", device);        
        
        // 设置性能提示
        ov::AnyMap properties;
        if (config.num_requests > 1) {
            properties[ov::hint::performance_mode.name()] = 
                ov::hint::PerformanceMode::THROUGHPUT;
            properties[ov::hint::num_requests.name()] = config.num_requests;
        } else {
            properties[ov::hint::performance_mode.name()] = 
                ov::hint::PerformanceMode::LATENCY;
        }
        
        LOG_MAIN_DEBUG_AT("Compiling model...");
        compiled_model_ = core_.compile_model(model_ptr, device, properties);
        LOG_MAIN_INFO_AT("Model compiled successfully");
        
        // 4. 创建推理请求池
        int num_requests = config.num_requests > 0 ? config.num_requests : 1;
        LOG_MAIN_INFO_AT("Creating {} inference requests", num_requests);
        infer_requests_.resize(num_requests);
        
        // std::mutex 不能被拷贝，使用 unique_ptr
        infer_request_mutexes_.clear();
        infer_request_mutexes_.reserve(num_requests);
        for (int i = 0; i < num_requests; ++i) {
            infer_requests_[i] = compiled_model_.create_infer_request();
            infer_request_mutexes_.push_back(std::make_unique<std::mutex>());
        }
        
        initialized_ = true;
        LOG_MAIN_INFO_AT("OpenVINO model loaded successfully");
        
        // 打印模型信息
        auto input_info = GetInputInfo();
        auto output_info = GetOutputInfo();
        LOG_MAIN_INFO_AT("Model input shape: {}", 
                   input_info.empty() ? "N/A" : 
                   [this]() {
                       std::string shape_str;
                       for (auto& info : GetInputInfo()) {
                           shape_str += "[";
                           for (size_t i = 0; i < info.shape.size(); ++i) {
                               shape_str += std::to_string(info.shape[i]);
                               if (i < info.shape.size() - 1) shape_str += ",";
                           }
                           shape_str += "] ";
                       }
                       return shape_str;
                   }());
        
        // 4. 启动异步工作线程（如果启用异步模式）
        // 注意：必须在 initialized_ = true 之后启动，确保所有资源已就绪
        if (async_mode_) {
            // 如果已有线程在运行，先停止它
            if (worker_thread_.joinable()) {
                LOG_MAIN_INFO_AT("Stopping existing worker thread...");
                running_ = false;
                queue_cv_.notify_all();
                worker_thread_.join();
                LOG_MAIN_INFO_AT("Existing worker thread stopped"); 
            }
            
            // 重新启动线程
            running_ = true;
            LOG_MAIN_INFO_AT("Starting new worker thread...");
            worker_thread_ = std::thread(&OpenVinoCpuEngine::WorkerLoop, this);
            LOG_MAIN_DEBUG_AT("Async worker thread started");
        }
        
        return true;
        
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("Failed to load OpenVINO model: {}", e.what());
        initialized_ = false;
        return false;
    }
}

InferenceOutput OpenVinoCpuEngine::Infer(const TensorData& input) {
    if (!initialized_) {
        return InferenceOutput{
            .success = false,
            .error_message = "Engine not initialized"
        };
    }
    
    return ExecuteInference(input);
}

bool OpenVinoCpuEngine::InferAsync(const TensorData& input, 
                                  InferenceCallback callback) {
    if (!initialized_) {
        if (callback) {
            callback(InferenceOutput{
                .success = false,
                .error_message = "Engine not initialized"
            });
        }
        return false;
    }
    
    if (!async_mode_) {
        LOG_MAIN_WARN_AT("Async mode is disabled, falling back to sync inference");
        auto result = Infer(input);
        if (callback) {
            callback(result);
        }
        return result.success;
    }
    
    // 将任务加入队列
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        task_queue_.push({input, callback});
    }
    queue_cv_.notify_one();
    
    return true;
}

std::vector<InferenceOutput> OpenVinoCpuEngine::InferBatch(
    const std::vector<TensorData>& inputs) {
    std::vector<InferenceOutput> results;
    results.reserve(inputs.size());
    
    for (const auto& input : inputs) {
        results.push_back(Infer(input));
    }
    
    return results;
}

bool OpenVinoCpuEngine::WaitAll() {
    if (!async_mode_) {
        return true;
    }
    
    // 等待任务队列清空
    std::unique_lock<std::mutex> lock(queue_mutex_);
    queue_cv_.wait(lock, [this]() {
        return task_queue_.empty();
    });
    
    return true;
}

std::vector<IInferenceEngine::TensorInfo> OpenVinoCpuEngine::GetInputInfo() const {
    if (!initialized_) {
        return {};
    }
    
    std::vector<TensorInfo> infos;
    auto inputs = compiled_model_.inputs();
    
    for (const auto& input : inputs) {
        TensorInfo info;
        info.name = input.get_any_name();
        
        auto shape = input.get_shape();
        info.shape.assign(shape.begin(), shape.end());
        
        auto element_type = input.get_element_type();
        info.dtype = element_type.get_type_name();
        
        infos.push_back(info);
    }
    
    return infos;
}

std::vector<IInferenceEngine::TensorInfo> OpenVinoCpuEngine::GetOutputInfo() const {
    if (!initialized_) {
        return {};
    }
    
    std::vector<TensorInfo> infos;
    auto outputs = compiled_model_.outputs();
    
    for (const auto& output : outputs) {
        TensorInfo info;
        info.name = output.get_any_name();
        
        auto shape = output.get_shape();
        info.shape.assign(shape.begin(), shape.end());
        
        auto element_type = output.get_element_type();
        info.dtype = element_type.get_type_name();
        
        infos.push_back(info);
    }
    
    return infos;
}

IInferenceEngine::Stats OpenVinoCpuEngine::GetStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    Stats stats;
    stats.inferences_count = total_inferences_;
    stats.errors_count = total_errors_;
    
    if (total_inferences_ > 0) {
        stats.avg_inference_time_ms = total_time_ms_ / total_inferences_;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - start_time_).count();
    
    if (elapsed > 0) {
        stats.fps = static_cast<double>(total_inferences_) / elapsed;
    }
    
    return stats;
}

void OpenVinoCpuEngine::WorkerLoop() {
    while (running_) {
        AsyncTask task;
        
        // 从队列获取任务
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this]() {
                return !task_queue_.empty() || !running_;
            });
            
            if (!running_ && task_queue_.empty()) {
                break;
            }
            
            task = std::move(task_queue_.front());
            task_queue_.pop();
        }
        
        // 执行推理
        auto result = ExecuteInference(task.input);
        
        // 调用回调
        if (task.callback) {
            task.callback(result);
        }
    }
}

ov::Tensor OpenVinoCpuEngine::CreateInputTensor(const TensorData& input) {
    // auto& cfg = config_.preprocess_config;
    // ov::Shape shape;

    // switch (cfg.input_format) {
    //     case ImageFormat::YUV420P: {
    //         shape = { 1, static_cast<size_t>(cfg.input_height * 3 / 2),
    //                 static_cast<size_t>(cfg.input_width), 1 };
    //         break;
    //     }

    //     case ImageFormat::NV12:
    //     case ImageFormat::NV21: {
    //         shape = { 1, static_cast<size_t>(cfg.input_height * 3 / 2),
    //                 static_cast<size_t>(cfg.input_width), 1 };
    //         break;
    //     }

    //     case ImageFormat::RGB:
    //     case ImageFormat::BGR: {
    //         shape = { 1, static_cast<size_t>(cfg.input_height),
    //                 static_cast<size_t>(cfg.input_width), 3 };
    //         break;
    //     }

    //     default: {
    //         LOG_MAIN_ERROR_AT("Unsupported format: {}", static_cast<int>(cfg.input_format));
    //         throw std::runtime_error("Unsupported format");
    //     }
    // }

    // return ov::Tensor(ov::element::u8, shape, input.data);
    
    /// 从模型输入获取shape, 不要手动设置shape，由preprocessor自动设置！！！
    auto shape = compiled_model_.input().get_shape();
    return ov::Tensor(ov::element::u8, shape, (void*)input.data);
}

InferenceOutput OpenVinoCpuEngine::ExecuteInference(const TensorData& input) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        // 1. 获取空闲的推理请求
        int request_idx = current_request_idx_.fetch_add(1) % infer_requests_.size();
        
        // 防止同一个request并发
        std::lock_guard<std::mutex> request_lock(*infer_request_mutexes_[request_idx]);
        auto& infer_request = infer_requests_[request_idx];
        
        // 2. 设置输入数据
        if (use_preprocessor_ && preprocessor_) {
            auto tensor = CreateInputTensor(input);
            infer_request.set_input_tensor(tensor);
        } else {
            auto tensor = infer_request.get_input_tensor();
            std::memcpy(tensor.data(), input.data, input.size_bytes);
        }        
        
        // 调试输出
        // LOG_MAIN_DEBUG_AT("ExecuteInference");
        // LOG_MAIN_DEBUG_AT("  request_idx: {}", request_idx);
        // LOG_MAIN_DEBUG_AT("  infer_requests_.size(): {}", infer_requests_.size());        
        // LOG_MAIN_DEBUG_AT("  input.data: {}", static_cast<const void*>(input.data));
        // LOG_MAIN_DEBUG_AT("  input.size_bytes: {}", input.size_bytes);
        // LOG_MAIN_DEBUG_AT("  input.dtype: {}", (input.dtype == TensorDataType::UINT8 ? "UINT8" : "FLOAT32"));
        
        // 安全检查
        if (!input.data) {
            LOG_MAIN_ERROR_AT("input data is NULL!");
            return InferenceOutput{
                .success = false,
                .error_message = "Invalid input data pointer"
            };
        }                
        
        // 3. 执行推理
        infer_request.infer();
        
        // 4. 获取输出
        InferenceOutput output;
        output.success = true;
        
        auto outputs = compiled_model_.outputs();

        for (const auto& output_info : outputs) {
            std::string name = output_info.get_any_name();
            auto output_tensor = infer_request.get_tensor(name);
            
            TensorData tensor_data;
            tensor_data.data = output_tensor.data();
            tensor_data.is_gpu = false;
            
            auto shape = output_tensor.get_shape();
            tensor_data.shape.assign(shape.begin(), shape.end());
            tensor_data.size_bytes = output_tensor.get_byte_size();

            output.tensors[name] = tensor_data;
        }
        
        // 5. 计算耗时
        auto end_time = std::chrono::high_resolution_clock::now();
        output.inference_time_us = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time).count();
        
        // 6. 更新统计
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            total_inferences_++;
            total_time_ms_ += output.inference_time_us / 1000.0;
        }
        
        return output;
        
    } catch (const std::exception& e) {
        LOG_MAIN_ERROR_AT("Inference failed: {}", e.what());
        
        // 更新错误统计
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            total_errors_++;
        }
        
        return InferenceOutput{
            .success = false,
            .error_message = e.what()
        };
    }
}
