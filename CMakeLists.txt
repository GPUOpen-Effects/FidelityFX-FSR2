# This file is part of the FidelityFX SDK.
# 
# Copyright (c) 2022 Advanced Micro Devices, Inc. All rights reserved.
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

cmake_minimum_required(VERSION 3.12.1)

option (GFX_API_DX11 "Build with DX11" ON)
option (GFX_API_DX12 "Build with DX12" ON)
option (GFX_API_VK "Build with Vulkan" ON)

if(NOT DEFINED GFX_API)
    project (FSR2_Sample)
else()
    project (FSR2_Sample_${GFX_API})

    set_property(DIRECTORY ${CMAKE_PROJECT_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})

    set(GFX_API_DX11 OFF)

    if(GFX_API STREQUAL DX12)
        set(GFX_API_DX12 ON)
        set(GFX_API_VK OFF)
    elseif(GFX_API STREQUAL VK)
        set(GFX_API_DX12 OFF)
        set(GFX_API_VK ON)
    else()
        message(STATUS "----------------------------------------------------------------------------------------")
        message(STATUS "")
        message(STATUS "** Almost there!!")
        message(STATUS "")
        message(STATUS " This framework supports DX12 and VULKAN, you need to invoke cmake in one of these ways:")
        message(STATUS "")
        message(STATUS " Examples:")
        message(STATUS "  Generate selected one:")
        message(STATUS "    cmake <project_root_dir> -DGFX_API=DX12")
        message(STATUS "    cmake <project_root_dir> -DGFX_API=VK")
        message(STATUS "  Generate with switches (Default is ON):")
        message(STATUS "    cmake <project_root_dir> [-DGFX_API_DX12=ON|OFF] [-DGFX_API_VK=ON|OFF]")
        message(STATUS "")
        message(STATUS "----------------------------------------------------------------------------------------")
        message(FATAL_ERROR "")
    endif()
endif()

# Check MSVC toolset version, Visual Studio 2019 required
if(MSVC_TOOLSET_VERSION VERSION_LESS 142)
    message(FATAL_ERROR "Cannot find MSVC toolset version 142 or greater. Please make sure Visual Studio 2019 or newer installed")
endif()

# ouput exe to bin directory
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_HOME_DIRECTORY}/bin)

foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
    string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
    set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_HOME_DIRECTORY}/bin )
endforeach( OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES )

add_compile_options(/MP)
add_compile_definitions($<$<CONFIG:RelWithDebInfo>:USE_PIX>)

# override build options in ffx-fsr2-api cmake
option (FFX_FSR2_API_DX11 "Build FSR 2.0 DX11 backend" ${GFX_API_DX11})
option (FFX_FSR2_API_DX12 "Build FSR 2.0 DX12 backend" ${GFX_API_DX12})
option (FFX_FSR2_API_VK "Build FSR 2.0 Vulkan backend" ${GFX_API_VK})

# reference libs used by both backends
add_subdirectory(libs/cauldron)
add_subdirectory(src/Common)
add_subdirectory(src/ffx-fsr2-api)

if(GFX_API_VK)
    find_package(Vulkan REQUIRED)
    add_subdirectory(src/VK)
endif()
if(GFX_API_DX12)
    add_subdirectory(src/DX12)
endif()
if(GFX_API_DX11)
    add_subdirectory(src/DX11)
endif()

