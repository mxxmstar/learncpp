# verify_modules.ps1
# 验证模块化重构是否完成

$ErrorActionPreference = "Continue"

$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  模块化重构验证" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

$Modules = @("log", "net", "puller", "sqlite", "zlmediakit", "config", "api", "service")
$AllPassed = $true

foreach ($Module in $Modules) {
    Write-Host "检查模块: $Module" -ForegroundColor Yellow
    
    $IncludePath = Join-Path $ProjectRoot "modules\$Module\include\$Module"
    $SrcPath = Join-Path $ProjectRoot "modules\$Module\src"
    
    # 检查 include 目录
    if (Test-Path $IncludePath) {
        $HeaderCount = (Get-ChildItem $IncludePath -File).Count
        Write-Host "  ✓ Include 目录存在 ($HeaderCount 个头文件)" -ForegroundColor Green
    } else {
        Write-Host "  ✗ Include 目录不存在: $IncludePath" -ForegroundColor Red
        $AllPassed = $false
    }
    
    # 检查 src 目录
    if (Test-Path $SrcPath) {
        $SrcCount = (Get-ChildItem $SrcPath -File -Filter "*.cpp").Count
        Write-Host "  ✓ Src 目录存在 ($SrcCount 个源文件)" -ForegroundColor Green
    } else {
        Write-Host "  ✗ Src 目录不存在: $SrcPath" -ForegroundColor Red
        $AllPassed = $false
    }
    
    # 检查 CMakeLists.txt
    $CMakePath = Join-Path $ProjectRoot "modules\$Module\CMakeLists.txt"
    if (Test-Path $CMakePath) {
        Write-Host "  ✓ CMakeLists.txt 存在" -ForegroundColor Green
    } else {
        Write-Host "  ✗ CMakeLists.txt 不存在" -ForegroundColor Red
        $AllPassed = $false
    }
    
    Write-Host ""
}

Write-Host "========================================" -ForegroundColor Cyan
if ($AllPassed) {
    Write-Host "  ✓ 所有模块验证通过！" -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "可以进行下一步：" -ForegroundColor Yellow
    Write-Host "1. 在 Visual Studio 中重新配置项目" -ForegroundColor White
    Write-Host "2. 编译项目验证是否能正常构建" -ForegroundColor White
    Write-Host "3. 确认成功后删除旧的 include/ 和 src/ 目录" -ForegroundColor White
} else {
    Write-Host "  ✗ 部分模块验证失败" -ForegroundColor Red
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "请检查上述错误并修复后重试" -ForegroundColor Yellow
    exit 1
}
