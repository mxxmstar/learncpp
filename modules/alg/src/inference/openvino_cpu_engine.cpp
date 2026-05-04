#include "alg/inference/openvino_cpu_engine.h"
#include "common/log/logmanager.h"
#include <iostream>
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
        config_ = config;
        async_mode_ = config.async_mode;
        
        // 1. 读取模型
        LOG_MAIN_INFO_AT("Loading OpenVINO model from: {}", config.model_path);
        auto model_ptr = core_.read_model(config.model_path);
        
        // 2. 编译模型
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
        
        compiled_model_ = core_.compile_model(model_ptr, device, properties);
        
        // 3. 创建推理请求池
        int num_requests = config.num_requests > 0 ? config.num_requests : 1;
        LOG_MAIN_INFO_AT("Creating {} inference requests", num_requests);
        infer_requests_.resize(num_requests);
        for (int i = 0; i < num_requests; ++i) {
            infer_requests_[i] = compiled_model_.create_infer_request();
        }
        
        // 4. 启动异步工作线程（如果启用异步模式）
        if (async_mode_) {
            running_ = true;
            worker_thread_ = std::thread(&OpenVinoCpuEngine::WorkerLoop, this);
            LOG_MAIN_INFO_AT("Async worker thread started");
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

InferenceOutput OpenVinoCpuEngine::ExecuteInference(const TensorData& input) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        // 1. 获取空闲的推理请求
        int request_idx = current_request_idx_.fetch_add(1) % infer_requests_.size();
        auto& infer_request = infer_requests_[request_idx];
        
        // 2. 设置输入数据
        auto input_tensor = infer_request.get_input_tensor();
        void* input_ptr = input_tensor.data();
        
        // 拷贝输入数据到推理缓冲区
        if (input.data && input.size_bytes > 0) {
            std::memcpy(input_ptr, input.data, input.size_bytes);
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
