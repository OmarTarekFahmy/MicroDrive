@echo off
REM Build script for MicroDrive (RP2040)
REM Requires: CMake, ARM GCC toolchain, and PICO_SDK_PATH environment variable

echo ======================================
echo    MicroDrive Build Script
echo    Raspberry Pi Pico (RP2040)
echo ======================================
echo.

REM Check if PICO_SDK_PATH is set
if not defined PICO_SDK_PATH (
    echo ERROR: PICO_SDK_PATH environment variable is not set!
    echo Please set it to your Pico SDK installation directory.
    echo Example: set PICO_SDK_PATH=C:\pico-sdk
    exit /b 1
)

echo Using Pico SDK: %PICO_SDK_PATH%
echo.

REM Create build directory if it doesn't exist
if not exist "build" (
    echo Creating build directory...
    mkdir build
)

REM Navigate to build directory
cd build

REM Run CMake configuration
echo Running CMake configuration...
cmake -G "NMake Makefiles" ..\src
if %errorlevel% neq 0 (
    echo ERROR: CMake configuration failed!
    cd ..
    exit /b 1
)

echo.
echo Building project...
nmake
if %errorlevel% neq 0 (
    echo ERROR: Build failed!
    cd ..
    exit /b 1
)

echo.
echo ======================================
echo Build successful!
echo ======================================
echo.
echo Output files:
dir /b *.uf2 *.elf *.bin 2>nul

echo.
echo To flash your Pico:
echo 1. Hold BOOTSEL button while plugging in USB
echo 2. Copy build\micro_drive.uf2 to the RPI-RP2 drive
echo.

cd ..
