Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path $MyInvocation.MyCommand.Path -Parent
$ProjectRoot = Resolve-Path "$ScriptDir/.."

if (!(Test-Path "$ProjectRoot/vcpkg")) {
    Write-Host "Cloning vcpkg..."
    git clone https://github.com/microsoft/vcpkg.git "$ProjectRoot/vcpkg"
    & "$ProjectRoot/vcpkg/bootstrap-vcpkg.bat"
}

Write-Host "vcpkg is ready at $ProjectRoot/vcpkg"
Write-Host "Dependencies will be installed automatically on first CMake run."