#!/bin/bash
# ============================================
# The Rocket - macOS Internal UI Build Script
# ============================================
# Builds with the internal preset designer UI enabled

set -e

echo "============================================"
echo "The Rocket - macOS Internal UI Build Script"
echo "============================================"

cd "$(dirname "$0")"

if ! command -v cmake &> /dev/null; then
    echo "ERROR: CMake not found"
    exit 1
fi

mkdir -p build_mac_internal

echo ""
echo "Configuring CMake with ROCKET_INTERNAL_UI=ON..."
cmake -S . -B build_mac_internal -G Xcode -DROCKET_INTERNAL_UI=ON

if [ $? -ne 0 ]; then
    echo "ERROR: CMake configuration failed"
    exit 1
fi

echo ""
echo "Building Release Standalone with Internal UI..."
cmake --build build_mac_internal --config Release --target TheRocket_Standalone

if [ $? -ne 0 ]; then
    echo "ERROR: Build failed"
    exit 1
fi

echo ""
echo "============================================"
echo "INTERNAL UI BUILD SUCCESSFUL!"
echo "============================================"
echo ""
echo "Output: build_mac_internal/TheRocket_artefacts/Release/Standalone/The Rocket.app"
echo ""

exit 0
