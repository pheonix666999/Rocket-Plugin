#!/bin/bash
# ============================================
# The Rocket - macOS Build Script
# ============================================
# This script builds The Rocket plugin for macOS
# Requires: CMake, Xcode

set -e

echo "============================================"
echo "The Rocket - macOS Build Script"
echo "============================================"

# Navigate to script directory
cd "$(dirname "$0")"

# Check for CMake
if ! command -v cmake &> /dev/null; then
    echo "ERROR: CMake not found"
    echo "Please install CMake: brew install cmake"
    exit 1
fi

# Create build directory
mkdir -p build_mac

# Configure
echo ""
echo "Configuring CMake..."
cmake -S . -B build_mac -G Xcode

if [ $? -ne 0 ]; then
    echo "ERROR: CMake configuration failed"
    exit 1
fi

# Build AU
echo ""
echo "Building Release AU..."
cmake --build build_mac --config Release --target TheRocket_AU

if [ $? -ne 0 ]; then
    echo "ERROR: AU build failed"
    exit 1
fi

# Build VST3
echo ""
echo "Building Release VST3..."
cmake --build build_mac --config Release --target TheRocket_VST3

if [ $? -ne 0 ]; then
    echo "ERROR: VST3 build failed"
    exit 1
fi

# Build Standalone
echo ""
echo "Building Release Standalone..."
cmake --build build_mac --config Release --target TheRocket_Standalone

if [ $? -ne 0 ]; then
    echo "ERROR: Standalone build failed"
    exit 1
fi

echo ""
echo "============================================"
echo "BUILD SUCCESSFUL!"
echo "============================================"
echo ""
echo "Output files:"
echo "  AU: build_mac/TheRocket_artefacts/Release/AU/The Rocket.component"
echo "  VST3: build_mac/TheRocket_artefacts/Release/VST3/The Rocket.vst3"
echo "  Standalone: build_mac/TheRocket_artefacts/Release/Standalone/The Rocket.app"
echo ""

exit 0
