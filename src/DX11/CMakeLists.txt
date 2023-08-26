# This file is part of the FidelityFX SDK.
#
# Copyright (c) 2022-2023 Advanced Micro Devices, Inc. All rights reserved.
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

add_subdirectory(framework/dxut)
add_subdirectory(framework/amd_sdk)
add_subdirectory(framework/amd_lib)

set(FSR2_Sample_DX11_SOURCES
    FSR2DX11Sample.cpp
    FSR2DX11Sample.h
    Fsr2Wrapper.cpp
    Fsr2Wrapper.h
    ResourceFiles/GPUOpenIcon.rc
    ResourceFiles/dpiaware.manifest
)

set(FSR2_Sample_DX11_SHADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/FSR2DX11Sample.hlsl
)
set(FSR2_Sample_DX11_CONFIG
    ${CMAKE_CURRENT_SOURCE_DIR}/FSR2_Sample_DX11.json
)

copyTargetCommand("${FSR2_Sample_DX11_SHADERS}" ${CMAKE_HOME_DIRECTORY}/bin/ShaderLibDX/dx11 dx11_sample_shaders)
copyTargetCommand("${FSR2_Sample_DX11_CONFIG}" ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} dx11_sample_config)

add_executable(FSR2_Sample_DX11 WIN32 ${FSR2_Sample_DX11_SOURCES} ${FSR2_Sample_DX11_SHADERS})
target_link_libraries(FSR2_Sample_DX11 PUBLIC comctl32 Usp10 NJSON amd_lib_minimal amd_sdk_minimal d3dcompiler D3D11 ffx_fsr2_api_x64 ffx_fsr2_api_dx11_x64)
set_target_properties(FSR2_Sample_DX11 PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_HOME_DIRECTORY}/bin" DEBUG_POSTFIX "d")
target_compile_definitions(FSR2_Sample_DX11 PRIVATE _UNICODE UNICODE)
add_dependencies(FSR2_Sample_DX11 dx11_sample_shaders dx11_sample_config)
