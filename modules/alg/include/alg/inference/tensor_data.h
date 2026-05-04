#pragma once

#include <cstdint>
#include <vector>
#include <string>

/// @brief 张量数据（支持 CPU/GPU）
struct TensorData {
    void* data = nullptr;       ///< 数据指针（CPU 或 GPU）
    std::vector<int64_t> shape; ///< 形状 [N, C, H, W]
    bool is_gpu = false;        ///< 是否在 GPU 上
    size_t size_bytes = 0;      ///< 数据大小（字节）
    
    TensorData() = default;
    
    /// @brief CPU 张量
    static TensorData FromCpu(const std::vector<float>& data, 
                             const std::vector<int64_t>& shape) {
        TensorData tensor;
        tensor.data = const_cast<float*>(data.data());
        tensor.shape = shape;
        tensor.is_gpu = false;
        tensor.size_bytes = data.size() * sizeof(float);
        return tensor;
    }
    
    /// @brief GPU 张量
    static TensorData FromGpu(void* gpu_ptr, 
                             const std::vector<int64_t>& shape,
                             size_t size_bytes) {
        TensorData tensor;
        tensor.data = gpu_ptr;
        tensor.shape = shape;
        tensor.is_gpu = true;
        tensor.size_bytes = size_bytes;
        return tensor;
    }
    
    /// @brief 计算元素总数
    int64_t NumElements() const {
        int64_t total = 1;
        for (auto dim : shape) {
            total *= dim;
        }
        return total;
    }
};
