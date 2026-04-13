# gRPC 代码生成脚本
# 用于手动生成 .proto 文件的 C++ 代码

# 使用 vcpkg 安装的 protoc 和插件（确保版本一致）
$PROTOC = "out/build/x64-Debug/vcpkg_installed/x64-windows/tools/protobuf/protoc.exe"
$PLUGIN = "out/build/x64-Debug/vcpkg_installed/x64-windows/tools/grpc/grpc_cpp_plugin.exe"

Write-Host "Protoc: $PROTOC"
Write-Host "Plugin: $PLUGIN"

# 检查文件是否存在
if (-not (Test-Path $PROTOC)) {
    Write-Error "protoc not found at $PROTOC"
    exit 1
}

if (-not (Test-Path $PLUGIN)) {
    Write-Error "grpc_cpp_plugin not found at $PLUGIN"
    exit 1
}

# 输出版本信息
Write-Host "`nProtoc version:"
& $PROTOC --version

# 输出目录
$grpcOutputDir = "modules/grpc/generated"        # grpc 模块的生成文件
$algOutputDir = "modules/alg/generated"           # alg 模块的生成文件

# 确保目录存在
@($grpcOutputDir, $algOutputDir) | ForEach-Object {
    if (-not (Test-Path $_)) {
        New-Item -ItemType Directory -Force -Path $_ | Out-Null
        Write-Host "Created output directory: $_"
    }
}

# 生成代码
Write-Host "`nGenerating gRPC code..."

# 1. 生成 hello.proto → grpc 模块
Write-Host "`n[1/2] Generating hello.proto..."
& $PROTOC `
  --proto_path=grpc/proto `
  --cpp_out=$grpcOutputDir `
  --grpc_out=$grpcOutputDir `
  --plugin=protoc-gen-grpc=$PLUGIN `
  grpc/proto/hello.proto

if ($LASTEXITCODE -ne 0) {
    Write-Host "Failed to generate hello.proto!" -ForegroundColor Red
    exit 1
}

# 2. 生成 video_processing.proto → alg 模块
Write-Host "`n[2/2] Generating video_processing.proto..."
& $PROTOC `
  --proto_path=modules/alg/proto `
  --cpp_out=$algOutputDir `
  --grpc_out=$algOutputDir `
  --plugin=protoc-gen-grpc=$PLUGIN `
  modules/alg/proto/video_processing.proto

if ($LASTEXITCODE -ne 0) {
    Write-Host "Failed to generate video_processing.proto!" -ForegroundColor Red
    exit 1
}

Write-Host "`nCode generation successful!" -ForegroundColor Green
Write-Host "`nGenerated files in grpc module:"
Get-ChildItem $grpcOutputDir | ForEach-Object {
    Write-Host "  - $($_.Name)"
}
Write-Host "`nGenerated files in alg module:"
Get-ChildItem $algOutputDir | ForEach-Object {
    Write-Host "  - $($_.Name)"
}
