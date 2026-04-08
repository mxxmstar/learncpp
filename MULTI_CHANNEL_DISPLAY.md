# 多通道分屏显示技术方案

## 📋 目录
1. [方案概述](#方案概述)
2. [技术架构](#技术架构)
3. [实现方案](#实现方案)
4. [性能优化](#性能优化)
5. [代码示例](#代码示例)

---

## 方案概述

### 什么是多通道分屏显示？

将多个视频通道的画面同时显示在一个窗口中，形成类似监控系统的多宫格布局。

```
┌─────────────┬─────────────┐
│  Channel 1  │  Channel 2  │
│   (主路)     │   (辅路)     │
├─────────────┼─────────────┤
│  Channel 3  │  Channel 4  │
│   (辅路)     │   (辅路)     │
└─────────────┴─────────────┘
```

### 应用场景

- **视频监控中心**：同时监控多个摄像头
- **智能分析平台**：对比不同算法效果
- **直播聚合**：多路直播同时观看
- **测试调试**：对比处理前后的差异

---

## 技术架构

### 核心组件

```
┌──────────────────────────────────────────┐
│         MultiChannelDisplay              │
├──────────────────────────────────────────┤
│  ┌──────────┐  ┌──────────┐             │
│  │Channel 1 │  │Channel 2 │  ...        │
│  │Pipeline  │  │Pipeline  │             │
│  └────┬─────┘  └────┬─────┘             │
│       │              │                    │
│  ┌────▼──────────────▼─────┐             │
│  │   Frame Aggregator      │  ← 帧收集器  │
│  └────────────┬────────────┘             │
│               │                           │
│  ┌────────────▼────────────┐             │
│  │   Grid Layout Engine    │  ← 布局引擎  │
│  └────────────┬────────────┘             │
│               │                           │
│  ┌────────────▼────────────┐             │
│  │   OpenCV Display Window │  ← 显示窗口  │
│  └─────────────────────────┘             │
└──────────────────────────────────────────┘
```

### 数据流

```
Camera 1 → Puller → Decoder → Processor → ┐
                                            ├→ Aggregator → Layout → Display
Camera 2 → Puller → Decoder → Processor → ┘
```

---

## 实现方案

### 方案 1：简单拼接（推荐入门）

**原理**：使用 `cv::hconcat` 和 `cv::vconcat` 拼接图像

**优点**：
- ✅ 实现简单
- ✅ 性能好
- ✅ 易于理解

**缺点**：
- ❌ 所有通道必须相同分辨率
- ❌ 布局固定（2x2, 3x3 等）

**适用场景**：
- 通道数量少（≤ 9）
- 分辨率统一

---

### 方案 2：ROI 绘制（推荐生产）

**原理**：在大画布上指定每个通道的 ROI（Region of Interest）区域

**优点**：
- ✅ 支持不同分辨率
- ✅ 灵活布局（不等分、重叠）
- ✅ 支持动态调整

**缺点**：
- ⚠️ 实现稍复杂
- ⚠️ 需要管理画布内存

**适用场景**：
- 通道数量多（≥ 4）
- 需要自定义布局
- 不同分辨率混合

---

### 方案 3：GPU 加速（高性能）

**原理**：使用 OpenGL/DirectX 渲染多个纹理

**优点**：
- ✅ 极高帧率（60+ FPS）
- ✅ 支持硬件加速
- ✅ 支持特效（缩放、旋转）

**缺点**：
- ❌ 依赖 GPU
- ❌ 实现复杂
- ❌ 跨平台兼容性差

**适用场景**：
- 超高清视频（4K+）
- 超多通道（≥ 16）
- 需要实时特效

---

## 性能优化

### 1. 帧率控制

```cpp
// 问题：所有通道同时刷新会导致卡顿
// 解决：错开刷新时间

if (channel_id == 1 && frame_count % 3 == 0) update();
if (channel_id == 2 && frame_count % 3 == 1) update();
if (channel_id == 3 && frame_count % 3 == 2) update();
```

### 2. 分辨率统一

```cpp
// 将所有通道缩放到相同尺寸
cv::resize(frame, resized_frame, cv::Size(640, 480));
```

### 3. 异步更新

```cpp
// 在后台线程准备拼接图像
std::thread layout_thread([&]() {
    composite_image = createGridLayout(frames);
});

// 主线程只负责显示
cv::imshow("Multi-View", composite_image);
```

### 4. 内存池

```cpp
// 预分配画布，避免频繁 malloc
cv::Mat canvas(1080 * 2, 1920 * 2, CV_8UC3);  // 2x2 布局
```

---

## 代码示例

### 示例 1：2x2 简单拼接

```cpp
#include <opencv2/opencv.hpp>
#include <vector>

class SimpleGridLayout {
public:
    // 创建 2x2 网格
    cv::Mat create2x2Grid(const std::vector<cv::Mat>& frames) {
        if (frames.size() != 4) {
            return cv::Mat();
        }
        
        // 确保所有帧大小一致
        cv::Size target_size(640, 480);
        std::vector<cv::Mat> resized_frames;
        for (const auto& frame : frames) {
            cv::Mat resized;
            cv::resize(frame, resized, target_size);
            resized_frames.push_back(resized);
        }
        
        // 水平拼接
        cv::Mat top_row, bottom_row;
        cv::hconcat(resized_frames[0], resized_frames[1], top_row);
        cv::hconcat(resized_frames[2], resized_frames[3], bottom_row);
        
        // 垂直拼接
        cv::Mat grid;
        cv::vconcat(top_row, bottom_row, grid);
        
        return grid;
    }
};
```

### 示例 2：灵活的 ROI 布局

```cpp
#include <opencv2/opencv.hpp>
#include <map>

struct ChannelLayout {
    int channel_id;
    cv::Rect roi;          // 在画布上的位置
    cv::Size target_size;  // 缩放目标尺寸
};

class FlexibleGridLayout {
public:
    FlexibleGridLayout(int canvas_width, int canvas_height) 
        : canvas_width_(canvas_width), canvas_height_(canvas_height) {
        canvas_.create(canvas_height, canvas_width, CV_8UC3);
    }
    
    // 添加通道布局
    void addChannel(int channel_id, cv::Rect roi, cv::Size size) {
        layouts_[channel_id] = {channel_id, roi, size};
    }
    
    // 更新单个通道
    void updateChannel(int channel_id, const cv::Mat& frame) {
        auto it = layouts_.find(channel_id);
        if (it == layouts_.end()) return;
        
        const auto& layout = it->second;
        
        // 缩放到目标尺寸
        cv::Mat resized;
        cv::resize(frame, resized, layout.target_size);
        
        // 复制到画布的 ROI 区域
        resized.copyTo(canvas_(layout.roi));
        
        // 绘制边框
        cv::rectangle(canvas_, layout.roi, cv::Scalar(255, 255, 255), 2);
        
        // 绘制通道标签
        std::string label = "Ch " + std::to_string(channel_id);
        cv::putText(canvas_, label, 
                   cv::Point(layout.roi.x + 5, layout.roi.y + 20),
                   cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);
    }
    
    // 获取完整画布
    cv::Mat getCanvas() const {
        return canvas_;
    }
    
    // 清空画布
    void clear() {
        canvas_.setTo(cv::Scalar(0, 0, 0));
    }
    
private:
    int canvas_width_;
    int canvas_height_;
    cv::Mat canvas_;
    std::map<int, ChannelLayout> layouts_;
};

// 使用示例
void setupGridLayout(FlexibleGridLayout& grid) {
    // 2x2 布局，每个通道 640x480
    grid.addChannel(1, cv::Rect(0, 0, 640, 480), cv::Size(640, 480));
    grid.addChannel(2, cv::Rect(640, 0, 640, 480), cv::Size(640, 480));
    grid.addChannel(3, cv::Rect(0, 480, 640, 480), cv::Size(640, 480));
    grid.addChannel(4, cv::Rect(640, 480, 640, 480), cv::Size(640, 480));
}
```

### 示例 3：集成到 VideoPipeline

```cpp
#include "video_pipeline/video_pipeline.h"
#include "video_pipeline/processor/osd_renderer.h"

class MultiChannelViewer {
public:
    MultiChannelViewer(int rows, int cols) 
        : rows_(rows), cols_(cols),
          grid_layout_(cols * 640, rows * 480) {
        setupGridLayout();
    }
    
    // 注册通道
    void addChannel(int channel_id, boost::asio::io_context& io_ctx) {
        PipelineConfig config;
        config.channel_id = channel_id;
        config.stream_url = "http://127.0.0.1/live/cam" + 
                           std::to_string(channel_id) + ".flv";
        
        auto pipeline = std::make_shared<VideoPipeline>(io_ctx, config);
        pipelines_[channel_id] = pipeline;
        
        // 设置回调：更新到网格
        pipeline->setFrameOutputCallback(
            [this, channel_id](int ch, cv::Mat&& frame, int64_t pts) {
                // 应用 OSD
                osd_renderer_.render(frame, ch, pts, 30.0f);
                
                // 更新到网格布局
                grid_layout_.updateChannel(channel_id, frame);
                
                // 显示（每 3 帧刷新一次）
                static int count = 0;
                if (++count % 3 == 0) {
                    cv::imshow("Multi-Channel View", 
                              grid_layout_.getCanvas());
                    cv::waitKey(1);
                }
            }
        );
        
        // 启动流水线
        pipeline->start();
    }
    
    // 运行
    void run() {
        while (true) {
            int key = cv::waitKey(10);
            if (key == 27 || key == 'q') break;  // ESC 或 q 退出
        }
        
        // 停止所有流水线
        for (auto& [id, pipeline] : pipelines_) {
            pipeline->stop();
        }
        
        cv::destroyAllWindows();
    }
    
private:
    void setupGridLayout() {
        int cell_width = 640;
        int cell_height = 480;
        
        for (int row = 0; row < rows_; ++row) {
            for (int col = 0; col < cols_; ++col) {
                int ch_id = row * cols_ + col + 1;
                cv::Rect roi(col * cell_width, row * cell_height,
                            cell_width, cell_height);
                grid_layout_.addChannel(ch_id, roi, 
                                       cv::Size(cell_width, cell_height));
            }
        }
    }
    
    int rows_;
    int cols_;
    FlexibleGridLayout grid_layout_;
    OsdRenderer osd_renderer_;
    std::map<int, std::shared_ptr<VideoPipeline>> pipelines_;
};

// 主函数
int main() {
    boost::asio::io_context io_ctx;
    
    // 创建 2x2 查看器
    MultiChannelViewer viewer(2, 2);
    
    // 添加 4 个通道
    viewer.addChannel(1, io_ctx);
    viewer.addChannel(2, io_ctx);
    viewer.addChannel(3, io_ctx);
    viewer.addChannel(4, io_ctx);
    
    // 启动 io_context
    std::thread io_thread([&io_ctx]() {
        io_ctx.run();
    });
    
    // 运行查看器
    viewer.run();
    
    // 清理
    io_ctx.stop();
    io_thread.join();
    
    return 0;
}
```

---

## 布局模板

### 1x1 单通道
```
┌──────────────┐
│              │
│   Channel 1  │
│              │
└──────────────┘
```

### 2x2 四通道（最常用）
```
┌──────┬──────┐
│ Ch 1 │ Ch 2 │
├──────┼──────┤
│ Ch 3 │ Ch 4 │
└──────┴──────┘
```

### 3x3 九通道
```
┌────┬────┬────┐
│ 1  │ 2  │ 3  │
├────┼────┼────┤
│ 4  │ 5  │ 6  │
├────┼────┼────┤
│ 7  │ 8  │ 9  │
└────┴────┴────┘
```

### 1+3 主次布局
```
┌────────┬────┐
│        │ 2  │
│   1    ├────┤  ← Ch 1 主画面（大）
│        │ 3  │
└────────┴────┘
```

### 自定义布局
```
┌──────┬───┐
│      │ 2 │
│  1   ├───┤
│      │ 3 │
├──────┴───┤
│     4    │  ← 底部宽屏
└──────────┘
```

---

## 常见问题

### Q1: 画面不同步怎么办？

**A**: 使用 PTS（Presentation Time Stamp）对齐：
```cpp
// 缓存帧，按 PTS 排序
std::map<int64_t, cv::Mat> frame_buffer;

void onFrameReceived(int ch, cv::Mat frame, int64_t pts) {
    frame_buffer[pts] = frame;
    
    // 当所有通道都有同一时刻的帧时，再显示
    if (frame_buffer.size() >= total_channels) {
        displaySynchronizedFrames();
        frame_buffer.clear();
    }
}
```

### Q2: 某个通道断流了怎么办？

**A**: 显示占位符：
```cpp
if (frame.empty()) {
    // 绘制黑色背景 + "No Signal" 文字
    cv::Mat placeholder(480, 640, CV_8UC3, cv::Scalar(0, 0, 0));
    cv::putText(placeholder, "No Signal", 
               cv::Point(250, 240), cv::FONT_HERSHEY_SIMPLEX,
               1.0, cv::Scalar(255, 255, 255), 2);
    grid_layout_.updateChannel(channel_id, placeholder);
}
```

### Q3: 如何支持点击切换主画面？

**A**: 使用鼠标回调：
```cpp
void onMouse(int event, int x, int y, int flags, void* userdata) {
    if (event == cv::EVENT_LBUTTONDOWN) {
        // 计算点击的是哪个通道
        int col = x / 640;
        int row = y / 480;
        int clicked_channel = row * 2 + col + 1;
        
        // 切换到该通道为主画面
        swapToMainView(clicked_channel);
    }
}

cv::setMouseCallback("Multi-Channel View", onMouse, nullptr);
```

---

## 总结

| 方案 | 复杂度 | 性能 | 灵活性 | 推荐场景 |
|------|--------|------|--------|----------|
| 简单拼接 | ⭐ | ⭐⭐⭐ | ⭐⭐ | ≤4 通道，统一分辨率 |
| ROI 布局 | ⭐⭐ | ⭐⭐⭐ | ⭐⭐⭐⭐ | 生产环境首选 |
| GPU 加速 | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | 超高清/超多通道 |

**推荐路线**：
1. 先用 **简单拼接** 快速验证功能
2. 再升级到 **ROI 布局** 满足生产需求
3. 如果需要极致性能，考虑 **GPU 加速**
