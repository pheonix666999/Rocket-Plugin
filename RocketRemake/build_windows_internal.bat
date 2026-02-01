@echo off
REM ============================================
REM The Rocket - Windows Internal UI Build Script
REM ============================================
REM Builds with the internal preset designer UI enabled

setlocal enabledelayedexpansion

echo ============================================
echo The Rocket - Internal UI Build Script
echo ============================================

cd /d "%~dp0"

cmake --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: CMake not found in PATH
    exit /b 1
)

if not exist "build_win_internal" mkdir build_win_internal

echo.
echo Configuring CMake with ROCKET_INTERNAL_UI=ON...
cmake -S . -B build_win_internal -G "Visual Studio 17 2022" -A x64 -DROCKET_INTERNAL_UI=ON

if errorlevel 1 (
    cmake -S . -B build_win_internal -G "Visual Studio 16 2019" -A x64 -DROCKET_INTERNAL_UI=ON
)

if errorlevel 1 (
    echo ERROR: CMake configuration failed
    exit /b 1
)

echo.
echo Building Release Standalone with Internal UI...
cmake --build build_win_internal --config Release --target TheRocket_Standalone

if errorlevel 1 (
    echo ERROR: Build failed
    exit /b 1
)

echo.
echo ============================================
echo INTERNAL UI BUILD SUCCESSFUL!
echo ============================================
echo.
echo Output: build_win_internal\TheRocket_artefacts\Release\Standalone\The Rocket.exe
echo.

exit /b 0
