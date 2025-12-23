#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR/.."

if [ ! -d "$PROJECT_ROOT/vcpkg" ]; then
    echo "Cloning vcpkg..."
    # git clone https://github.com/microsoft/vcpkg.git "$PROJECT_ROOT/vcpkg"
    # git clone https://gitee.com/mirrors/vcpkg.git "$PROJECT_ROOT/vcpkg"
     git clone git@github.com:microsoft/vcpkg.git "$PROJECT_ROOT/vcpkg"
    "$PROJECT_ROOT/vcpkg/bootstrap-vcpkg.sh"
fi

echo "vcpkg is ready at $PROJECT_ROOT/vcpkg"
echo "Dependencies will be installed automatically on first CMake run."
