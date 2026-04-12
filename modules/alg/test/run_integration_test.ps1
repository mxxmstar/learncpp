# GrpcAlgorithmProcessor 集成测试快速开始脚本
# 
# 使用方法：
# 1. 先运行此脚本启动 Python 服务器（终端 1）
# 2. 然后在另一个终端运行 C++ 测试程序

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "GrpcAlgorithmProcessor Integration Test" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# 检查 Python 服务器是否正在运行
$port = 50052
$connection = New-Object System.Net.Sockets.TcpClient
try {
    $connection.Connect("localhost", $port)
    Write-Host "[OK] Python gRPC server is already running on port $port" -ForegroundColor Green
} catch {
    Write-Host "[INFO] Python gRPC server is not running" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Please start the Python server first:" -ForegroundColor Cyan
    Write-Host "  cd algorithm/grpc_server" -ForegroundColor White
    Write-Host "  python video_service.py --port $port" -ForegroundColor White
    Write-Host ""
    Write-Host "Press any key to continue after starting the server..." -ForegroundColor Yellow
    $null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
} finally {
    if ($connection.Connected) {
        $connection.Close()
    }
}

Write-Host ""
Write-Host "Starting C++ test program..." -ForegroundColor Cyan
Write-Host ""

# 运行 C++ 测试程序
$testExe = "..\..\bin\test_grpc_algorithm_integration.exe"
if (Test-Path $testExe) {
    & $testExe
} else {
    Write-Host "[ERROR] Test executable not found: $testExe" -ForegroundColor Red
    Write-Host ""
    Write-Host "Please build the project first:" -ForegroundColor Yellow
    Write-Host "  cd out/build/x64-Debug" -ForegroundColor White
    Write-Host "  cmake --build . --target test_grpc_algorithm_integration" -ForegroundColor White
}
