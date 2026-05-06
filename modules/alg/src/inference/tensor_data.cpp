#include "alg/inference/tensor_data.h"
#include "decoder/i_decoder.h"

TensorData TensorData::FromVideoFrame(const VideoFrame& frame,
                                     const std::vector<int64_t>& shape,
                                     TensorDataType dtype) {
    TensorData tensor;
    
    if (frame.data[0] == nullptr || frame.width == 0 || frame.height == 0) {
        // 空帧，返回无效张量
        return tensor;
    }
    
    // 直接使用 VideoFrame 的 Y 平面（对于 YUV420P）
    // 注意：这是零拷贝，直接引用 frame 的内存
    tensor.data = frame.data[0];
    tensor.shape = shape;
    tensor.is_gpu = false;
    tensor.dtype = dtype;
    
    // 计算实际大小
    if (dtype == TensorDataType::UINT8) {
        // YUV420P: Y + U/2 + V/2 = width * height * 3 / 2
        // 但这里我们只使用 Y 平面（灰度）或需要调用者指定正确的 shape
        if (shape.size() >= 3 && shape[1] == 1) {
            // 单通道（灰度）
            tensor.size_bytes = frame.linesize[0] * frame.height;
        } else if (shape.size() >= 3 && shape[1] == 3) {
            // 三通道（RGB/YUV），需要三个平面
            // 这里简化处理，假设调用者会正确处理
            tensor.size_bytes = frame.linesize[0] * frame.height * 3 / 2;
        } else {
            // 默认使用 Y 平面大小
            tensor.size_bytes = frame.linesize[0] * frame.height;
        }
    } else {
        // 其他类型，使用 Y 平面大小
        tensor.size_bytes = frame.linesize[0] * frame.height;
    }
    
    return tensor;
}
