#!/bin/bash
# ===================================================================
# VideoPipeline OpenVINO 测试运行脚本 (Linux/macOS)
# ===================================================================

echo ""
echo "========================================"
echo "VideoPipeline OpenVINO Test Runner"
echo "========================================"
echo ""

# 设置默认参数
STREAM_URL="${1:-http://127.0.0.1/live/proxy_cam1.live.flv}"
MODEL_PATH="${2:-}"
DEVICE="${3:-CPU}"
DURATION="${4:-60}"

echo "Configuration:"
echo "  Stream URL: $STREAM_URL"
echo "  Model Path: ${MODEL_PATH:-(not specified)}"
echo "  Device: $DEVICE"
echo "  Duration: ${DURATION}s"
echo ""

# 检查可执行文件是否存在
if [ ! -f "./bin/test_video_pipeline_openvino" ]; then
    echo "Error: test_video_pipeline_openvino not found in bin directory"
    echo "Please build the project first."
    exit 1
fi

# 赋予执行权限
chmod +x ./bin/test_video_pipeline_openvino

# 设置 OpenVINO 插件路径（关键！）
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
export OPENVINO_PLUGIN_PATHS="$SCRIPT_DIR/bin"
echo "Setting OPENVINO_PLUGIN_PATHS=$OPENVINO_PLUGIN_PATHS"
echo ""

echo "Starting test..."
echo ""

# 运行测试
if [ -z "$MODEL_PATH" ]; then
    ./bin/test_video_pipeline_openvino "$STREAM_URL" "" "$DEVICE" "$DURATION"
else
    ./bin/test_video_pipeline_openvino "$STREAM_URL" "$MODEL_PATH" "$DEVICE" "$DURATION"
fi

EXIT_CODE=$?
echo ""
echo "Test completed with exit code: $EXIT_CODE"
exit $EXIT_CODE