# gRPC 代码生成脚本
# 用于手动生成 .proto 文件的 C++ 代码
# 注意：现在 gRPC 是独立的库，代码生成由 grpc/CMakeLists.txt 自动处理

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

# 输出目录（在 grpc/generated 中）
$outputDir = "grpc/generated"

# 确保目录存在
if (-not (Test-Path $outputDir)) {
    New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
    Write-Host "Created output directory: $outputDir"
}

# 生成代码
Write-Host "`nGenerating gRPC code..."
& $PROTOC `
  --proto_path=grpc/proto `
  --cpp_out=$outputDir `
  --grpc_out=$outputDir `
  --plugin=protoc-gen-grpc=$PLUGIN `
  grpc/proto/hello.proto

if ($LASTEXITCODE -eq 0) {
    Write-Host "`nCode generation successful!" -ForegroundColor Green
    Write-Host "Generated files in: $outputDir"
    Get-ChildItem $outputDir | ForEach-Object {
        Write-Host "  - $($_.Name)"
    }
} else {
    Write-Host "`nCode generation failed!" -ForegroundColor Red
    exit 1
}
