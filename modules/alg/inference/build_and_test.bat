@echo off
REM ========================================
REM Inference 模块编译和测试脚本 (Windows)
REM ========================================

echo ========================================
echo Building Inference Module
echo ========================================
echo.

REM 设置构建目录
set BUILD_DIR=build
if not exist %BUILD_DIR% mkdir %BUILD_DIR%

REM 进入构建目录
cd %BUILD_DIR%

REM 配置 CMake
echo [1/3] Configuring CMake...
cmake .. -DBUILD_ALG_TESTS=ON ^
         -DCMAKE_BUILD_TYPE=Release ^
         || goto :error

REM 编译
echo.
echo [2/3] Building...
cmake --build . --target alg_inference --config Release ^
         || goto :error

REM 编译示例程序
echo.
echo [3/3] Building examples...
cmake --build . --target inference_example --config Release ^
         || goto :error

echo.
echo ========================================
echo Build completed successfully!
echo ========================================
echo.
echo To run tests:
echo   cd test
echo   test_inference.exe
echo.
echo To run examples:
echo   cd bin
echo   inference_example.exe
echo.

goto :end

:error
echo.
echo ========================================
echo Build failed!
echo ========================================
exit /b 1

:end
