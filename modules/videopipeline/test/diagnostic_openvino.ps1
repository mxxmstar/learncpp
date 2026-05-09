# OpenVINO Environment Diagnostic Script
# Run this script to diagnose OpenVINO loading issues

Write-Host "=== OpenVINO Environment Diagnostic ===" -ForegroundColor Cyan
Write-Host ""

# Get script directory
$SCRIPT_DIR = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $SCRIPT_DIR

# 1. Check DLL files
Write-Host "1. Checking OpenVINO DLLs..." -ForegroundColor Yellow
$dlls = Get-ChildItem "bin\*openvino*.dll" -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Name
if ($dlls.Count -eq 0) {
    Write-Host "   ✗ No OpenVINO DLLs found in bin directory!" -ForegroundColor Red
    Write-Host "   → Run: Copy-Item -Path 'out\build\x64-Debug\vcpkg_installed\x64-windows\bin\*openvino*.dll' -Destination 'bin\' -Force" -ForegroundColor Gray
} else {
    Write-Host "   ✓ Found $($dlls.Count) OpenVINO DLLs" -ForegroundColor Green
}

# 2. Check critical DLLs
Write-Host "`n2. Checking critical DLLs..." -ForegroundColor Yellow
$critical_dlls = @{
    "openvino_ir_frontend.dll" = "IR model parser (required for .xml files)"
    "openvino_intel_cpu_plugin.dll" = "CPU inference engine"
    "openvino.dll" = "Core OpenVINO library"
}

foreach ($dll in $critical_dlls.Keys) {
    if (Test-Path "bin\$dll") {
        Write-Host "   ✓ $dll" -ForegroundColor Green
        Write-Host "     $($critical_dlls[$dll])" -ForegroundColor DarkGray
    } else {
        Write-Host "   ✗ $dll MISSING!" -ForegroundColor Red
        Write-Host "     $($critical_dlls[$dll])" -ForegroundColor DarkGray
    }
}

# 3. Check model files
Write-Host "`n3. Checking model files..." -ForegroundColor Yellow
$model_files = @("yolov5s.xml", "yolov5s.bin")
foreach ($model in $model_files) {
    if (Test-Path "bin\$model") {
        $size = (Get-Item "bin\$model").Length
        Write-Host "   ✓ $model ($([math]::Round($size/1KB, 2)) KB)" -ForegroundColor Green
    } else {
        Write-Host "   ✗ $model MISSING!" -ForegroundColor Red
        Write-Host "     → Copy from algorithm\yolov5\ov_model\" -ForegroundColor Gray
    }
}

# 4. Check environment variables
Write-Host "`n4. Checking environment variables..." -ForegroundColor Yellow
if ($env:OPENVINO_PLUGIN_PATHS) {
    Write-Host "   ✓ OPENVINO_PLUGIN_PATHS is set" -ForegroundColor Green
    Write-Host "     Path: $env:OPENVINO_PLUGIN_PATHS" -ForegroundColor DarkGray
    
    # Verify the path exists
    if (Test-Path $env:OPENVINO_PLUGIN_PATHS) {
        Write-Host "     ✓ Path exists" -ForegroundColor Green
    } else {
        Write-Host "     ✗ Path does NOT exist!" -ForegroundColor Red
    }
} else {
    Write-Host "   ⚠ OPENVINO_PLUGIN_PATHS not set" -ForegroundColor Yellow
    Write-Host "     → Use run_openvino_test.bat to automatically set it" -ForegroundColor Gray
}

# 5. Check executable
Write-Host "`n5. Checking test executable..." -ForegroundColor Yellow
if (Test-Path "bin\test_video_pipeline_openvino.exe") {
    Write-Host "   ✓ test_video_pipeline_openvino.exe exists" -ForegroundColor Green
} else {
    Write-Host "   ✗ test_video_pipeline_openvino.exe NOT found!" -ForegroundColor Red
    Write-Host "     → Build the project first" -ForegroundColor Gray
}

# 6. Summary
Write-Host "`n=== Summary ===" -ForegroundColor Cyan

$issues = 0

if ($dlls.Count -eq 0) { $issues++ }
if (-not (Test-Path "bin\openvino_ir_frontend.dll")) { $issues++ }
if (-not (Test-Path "bin\openvino_intel_cpu_plugin.dll")) { $issues++ }
if (-not (Test-Path "bin\yolov5s.xml")) { $issues++ }
if (-not (Test-Path "bin\test_video_pipeline_openvino.exe")) { $issues++ }

if ($issues -eq 0) {
    Write-Host "✓ All checks passed! You can run the test." -ForegroundColor Green
    Write-Host "`nRun: .\run_openvino_test.bat" -ForegroundColor Cyan
} else {
    Write-Host "✗ Found $issues issue(s). Please fix them before running the test." -ForegroundColor Red
    Write-Host "`nSee TROUBLESHOOTING_OPENVINO.md for solutions." -ForegroundColor Yellow
}

Write-Host ""