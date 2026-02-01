@echo off
REM ============================================
REM The Rocket - Windows Build Script
REM ============================================
REM This script builds The Rocket plugin for Windows
REM Requires: CMake, Visual Studio 2022 (or 2019), JUCE

setlocal enabledelayedexpansion

echo ============================================
echo The Rocket - Windows Build Script
echo ============================================

REM Navigate to project directory
cd /d "%~dp0"

REM Check for CMake
cmake --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: CMake not found in PATH
    echo Please install CMake from https://cmake.org/
    exit /b 1
)

REM Create build directory
if not exist "build_win" mkdir build_win

REM Configure (default build without internal UI)
echo.
echo Configuring CMake...
cmake -S . -B build_win -G "Visual Studio 17 2022" -A x64

if errorlevel 1 (
    echo.
    echo Trying Visual Studio 2019...
    cmake -S . -B build_win -G "Visual Studio 16 2019" -A x64
)

if errorlevel 1 (
    echo ERROR: CMake configuration failed
    exit /b 1
)

REM Build Release
echo.
echo Building Release VST3...
cmake --build build_win --config Release --target TheRocket_VST3

if errorlevel 1 (
    echo ERROR: VST3 build failed
    exit /b 1
)

echo.
echo Building Release Standalone...
cmake --build build_win --config Release --target TheRocket_Standalone

if errorlevel 1 (
    echo ERROR: Standalone build failed
    exit /b 1
)

echo.
echo ============================================
echo BUILD SUCCESSFUL!
echo ============================================
echo.
echo Output files:
echo   VST3: build_win\TheRocket_artefacts\Release\VST3\The Rocket.vst3
echo   Standalone: build_win\TheRocket_artefacts\Release\Standalone\The Rocket.exe
echo.

exit /b 0
