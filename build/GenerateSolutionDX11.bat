@echo off
setlocal enabledelayedexpansion

echo Checking pre-requisites... 

:: Check if CMake is installed
cmake --version > nul 2>&1
if %errorlevel% NEQ 0 (
    echo Cannot find path to cmake. Is CMake installed? Exiting...
    exit /b -1
) else (
    echo    CMake      - Ready.
) 

:: Call CMake
mkdir DX11
cd DX11
cmake -A x64 ..\.. -DGFX_API_DX11=ON -DGFX_API_DX12=OFF -DGFX_API_VK=OFF
cd ..
