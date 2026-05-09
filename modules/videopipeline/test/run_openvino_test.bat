@echo off
REM ===================================================================
REM VideoPipeline OpenVINO 测试运行脚本 (Windows)
REM ===================================================================

echo.
echo ========================================
echo VideoPipeline OpenVINO Test Runner
echo ========================================
echo.

REM 设置默认参数
set STREAM_URL=http://127.0.0.1/live/proxy_cam1.live.flv
set MODEL_PATH=
set DEVICE=CPU
set DURATION=60

REM 解析命令行参数
if not "%1"=="" set STREAM_URL=%1
if not "%2"=="" set MODEL_PATH=%2
if not "%3"=="" set DEVICE=%3
if not "%4"=="" set DURATION=%4

echo Configuration:
echo   Stream URL: %STREAM_URL%
echo   Model Path: %MODEL_PATH%
echo   Device: %DEVICE%
echo   Duration: %DURATION%s
echo.

REM 检查可执行文件是否存在
if not exist "bin\test_video_pipeline_openvino.exe" (
    echo Error: test_video_pipeline_openvino.exe not found in bin directory
    echo Please build the project first.
    exit /b 1
)

REM 设置 OpenVINO 插件路径（关键！）
set SCRIPT_DIR=%~dp0
set OPENVINO_PLUGIN_PATHS=%SCRIPT_DIR%bin
echo Setting OPENVINO_PLUGIN_PATHS=%OPENVINO_PLUGIN_PATHS%
echo.

echo Starting test...
echo.

REM 运行测试
if "%MODEL_PATH%"=="" (
    bin\test_video_pipeline_openvino.exe %STREAM_URL% "" %DEVICE% %DURATION%
) else (
    bin\test_video_pipeline_openvino.exe %STREAM_URL% %MODEL_PATH% %DEVICE% %DURATION%
)

echo.
echo Test completed with exit code: %ERRORLEVEL%
exit /b %ERRORLEVEL%