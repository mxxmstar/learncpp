#pragma once

#include <cstdint>
#include <vector>
#include <string>

/// @brief 张量数据类型
enum class TensorDataType {
    UINT8,      ///< uint8_t (0-255)
    FLOAT32,    ///< float (0.0-1.0)
    INT32,      ///< int32
    FLOAT16     ///< half precision
};

/// @brief 前向声明
struct VideoFrame;

/// @brief 张量数据（支持 CPU/GPU）
struct TensorData {
    void* data = nullptr;               ///< 数据指针（CPU 或 GPU）
    std::vector<int64_t> shape;         ///< 形状 [N, C, H, W]
    bool is_gpu = false;                ///< 是否在 GPU 上
    size_t size_bytes = 0;              ///< 数据大小（字节）
    TensorDataType dtype = TensorDataType::FLOAT32;  ///< 数据类型
    
    TensorData() = default;
    
    /// @brief CPU 张量（float）
    static TensorData FromCpu(const std::vector<float>& data, 
                             const std::vector<int64_t>& shape) {
        TensorData tensor;
        tensor.data = const_cast<float*>(data.data());
        tensor.shape = shape;
        tensor.is_gpu = false;
        tensor.size_bytes = data.size() * sizeof(float);
        tensor.dtype = TensorDataType::FLOAT32;
        return tensor;
    }
    
    /// @brief CPU 张量（uint8_t）
    static TensorData FromCpuUint8(const uint8_t* data,
                                  const std::vector<int64_t>& shape,
                                  size_t size_bytes) {
        TensorData tensor;
        tensor.data = const_cast<uint8_t*>(data);
        tensor.shape = shape;
        tensor.is_gpu = false;
        tensor.size_bytes = size_bytes;
        tensor.dtype = TensorDataType::UINT8;
        return tensor;
    }
    
    /// @brief 从 VideoFrame 创建张量（零拷贝）
    /// @param frame 视频帧（YUV 格式）
    /// @param shape 期望的输出形状 [N, C, H, W]
    /// @param dtype 数据类型（默认 UINT8）
    /// @return 张量数据（直接引用 frame 的内存）
    /// @note 调用者必须保证 frame 的生命周期长于推理过程
    static TensorData FromVideoFrame(const VideoFrame& frame,
                                    const std::vector<int64_t>& shape,
                                    TensorDataType dtype = TensorDataType::UINT8);
    
    /// @brief GPU 张量
    static TensorData FromGpu(void* gpu_ptr, 
                             const std::vector<int64_t>& shape,
                             size_t size_bytes) {
        TensorData tensor;
        tensor.data = gpu_ptr;
        tensor.shape = shape;
        tensor.is_gpu = true;
        tensor.size_bytes = size_bytes;
        tensor.dtype = TensorDataType::FLOAT32;  // GPU 通常为 float
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
