@echo off
setlocal enabledelayedexpansion

:: Ensure a consistent working directory location. 
:: All referenced paths are relative to this.
pushd %~dp0

echo Checking pre-requisites... 

:: Check if CMake is installed
cmake --version > nul 2>&1
if %errorlevel% NEQ 0 (
    echo Cannot find path to cmake. Is CMake installed? Exiting...
    exit /b -1
) else (
    echo    CMake      - Ready.
) 

:: Check if submodule is initialized (first time) to avoid CMake file not found errors
if not exist ..\libs\cauldron\common.cmake (
    echo File: common.cmake  doesn't exist in '.\libs\cauldron\'  -  Initializing submodule... 

    :: attempt to initialize submodules
    pushd ..
    echo.
    git submodule update --init --recursive
    popd


    :: check if submodule initialized properly
    if not exist ..\libs\cauldron\common.cmake (
        echo.
        echo '..\libs\cauldron\common.cmake is still not there.'
        echo Could not initialize submodule. Make sure all the submodules are initialized and updated.
        echo Exiting...
        echo.
        exit /b -1 
    ) else (
        echo    Cauldron   - Ready.
    )
) else (
    echo    Cauldron   - Ready.
)

:: Call CMake
mkdir DX12
pushd DX12
cmake -A x64 ..\.. -DGFX_API=DX12
popd


:: Check if VULKAN_SDK is installed but don't bail out
if "%VULKAN_SDK%"=="" (
    echo Vulkan SDK is not installed -Environment variable VULKAN_SDK is not defined- : Please install the latest Vulkan SDK from LunarG.
) else (
    echo    Vulkan SDK - Ready : %VULKAN_SDK%

    mkdir VK
    pushd VK
    cmake -A x64 ..\.. -DGFX_API=VK
    popd
)


popd
