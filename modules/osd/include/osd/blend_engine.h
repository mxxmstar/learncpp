#pragma once

/// @brief 混合引擎基类
/// 
/// 定义通用像素混合接口，各格式（YUV/RGB 等）派生子类实现。
class BlendEngine {
public:
    virtual ~BlendEngine() = default;
    
    /// @brief 设置不透明度
    virtual void SetOpacity(float opacity) = 0;
    
    /// @brief 获取不透明度
    virtual float GetOpacity() const = 0;
};