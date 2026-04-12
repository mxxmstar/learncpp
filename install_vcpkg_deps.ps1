# install_vcpkg_deps.ps1
# 自动安装 vcpkg 依赖的脚本

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  安装 vcpkg 依赖包" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# 获取脚本所在目录
$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$vcpkgExe = Join-Path $ProjectRoot "vcpkg\vcpkg.exe"
$vcpkgJson = Join-Path $ProjectRoot "vcpkg.json"

# 检查 vcpkg.exe 是否存在
if (-Not (Test-Path $vcpkgExe)) {
    Write-Host "错误: 找不到 vcpkg.exe" -ForegroundColor Red
    Write-Host "路径: $vcpkgExe" -ForegroundColor Red
    exit 1
}

# 检查 vcpkg.json 是否存在
if (-Not (Test-Path $vcpkgJson)) {
    Write-Host "错误: 找不到 vcpkg.json" -ForegroundColor Red
    Write-Host "路径: $vcpkgJson" -ForegroundColor Red
    exit 1
}

Write-Host "项目目录: $ProjectRoot" -ForegroundColor Green
Write-Host "vcpkg 路径: $vcpkgExe" -ForegroundColor Green
Write-Host ""

# 执行 vcpkg install
Write-Host "开始安装依赖..." -ForegroundColor Yellow
Write-Host ""

& $vcpkgExe install

if ($LASTEXITCODE -eq 0) {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Green
    Write-Host "  ✓ 所有依赖安装成功！" -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Green
    Write-Host ""
    Write-Host "现在可以在 Visual Studio 中重新编译项目" -ForegroundColor Cyan
} else {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Red
    Write-Host "  ✗ 依赖安装失败" -ForegroundColor Red
    Write-Host "========================================" -ForegroundColor Red
    Write-Host ""
    Write-Host "请检查错误信息并修复后重试" -ForegroundColor Yellow
    exit 1
}
