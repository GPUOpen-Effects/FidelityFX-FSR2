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

file(GLOB_RECURSE AMD_SDK_SOURCES *.cpp)
file(GLOB_RECURSE AMD_SDK_HEADERS *.h)

set(amd_sdk_minimal_shaders
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Shaders/Line.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Shaders/Sprite.hlsl
)

copyCommand("${amd_sdk_minimal_shaders}" ${CMAKE_HOME_DIRECTORY}/bin/ShaderLibDX/dx11)
add_library(amd_sdk_minimal ${AMD_SDK_SOURCES} ${AMD_SDK_HEADERS} ${amd_sdk_minimal_shaders})
target_include_directories(amd_sdk_minimal PUBLIC inc)
target_compile_definitions(amd_sdk_minimal PRIVATE AMD_SDK_MINIMAL _UNICODE UNICODE)
target_link_libraries(amd_sdk_minimal PUBLIC DXUT DXUTOpt)

