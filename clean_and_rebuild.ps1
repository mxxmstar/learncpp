# 清理并重新配置 CMake
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Cleaning and Reconfiguring CMake" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# 删除 build 目录
if (Test-Path build) {
    Write-Host "Removing old build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force build
    Write-Host "✓ Build directory removed" -ForegroundColor Green
} else {
    Write-Host "ℹ No build directory found" -ForegroundColor Gray
}

Write-Host ""

# 创建新的 build 目录
Write-Host "Creating new build directory..." -ForegroundColor Yellow
New-Item -ItemType Directory -Path build | Out-Null
Write-Host "✓ Build directory created" -ForegroundColor Green

Write-Host ""

# 进入 build 目录
Set-Location build

# 重新配置 CMake
Write-Host "Configuring CMake..." -ForegroundColor Yellow
cmake ..

if ($LASTEXITCODE -ne 0) {
    Write-Host "✗ CMake configuration failed!" -ForegroundColor Red
    Set-Location ..
    exit 1
}

Write-Host "✓ CMake configured successfully" -ForegroundColor Green
Write-Host ""

# 编译
Write-Host "Building project..." -ForegroundColor Yellow
cmake --build . --config Debug

if ($LASTEXITCODE -ne 0) {
    Write-Host "✗ Build failed!" -ForegroundColor Red
    Set-Location ..
    exit 1
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host "Build completed successfully!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""

# 显示编译的文件
Write-Host "Compiled files:" -ForegroundColor Cyan
Get-ChildItem ..\apps -Recurse -Filter *.cpp | Select-Object FullName
Get-ChildItem ..\modules -Recurse -Filter *.cpp | Select-Object FullName

Write-Host ""
Write-Host "Output files:" -ForegroundColor Cyan
if (Test-Path ..\bin\MySelfContainedApp.exe) {
    Write-Host "  ✓ ..\bin\MySelfContainedApp.exe" -ForegroundColor Green
}
if (Test-Path ..\modules\log\lib\log_lib.lib) {
    Write-Host "  ✓ ..\modules\log\lib\log_lib.lib" -ForegroundColor Green
}

Write-Host ""
Set-Location ..
