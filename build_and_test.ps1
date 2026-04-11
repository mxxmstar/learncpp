# 编译并测试迁移后的模块
# 使用方法: .\build_and_test.ps1

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Building and Testing Migrated Modules" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# 创建 build 目录
if (-Not (Test-Path "build")) {
    New-Item -ItemType Directory -Path "build" | Out-Null
}

Set-Location build

# 配置 CMake，启用所有新模块的测试
Write-Host "Configuring CMake..." -ForegroundColor Yellow
cmake .. `
    -DBUILD_TESTS=ON `
    -DBUILD_VIDEO_PIPELINE_TESTS=ON `
    -DBUILD_PULLER_TESTS=ON `
    -DBUILD_DECODER_TESTS=ON `
    -DBUILD_PREPROCESS_TESTS=ON `
    -DBUILD_POSTPROCESS_TESTS=ON `
    -DBUILD_ALG_TESTS=ON `
    -DBUILD_GRPC_TESTS=ON

if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake configuration failed!" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "Building project..." -ForegroundColor Yellow
cmake --build . --config Debug

if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed!" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host "Build completed successfully!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""

# 列出可用的测试
Write-Host "Available tests:" -ForegroundColor Cyan
ctest -N

Write-Host ""
Write-Host "To run all tests: ctest -C Debug --verbose" -ForegroundColor Yellow
Write-Host "To run specific test: ctest -R <test_name> -C Debug --verbose" -ForegroundColor Yellow
Write-Host ""

Set-Location ..
