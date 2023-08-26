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

if(NOT ${FFX_FSR2_API_DX11})
    return()
endif()

set(FFX_SC_DX11_BASE_ARGS
    -E CS -DFFX_HLSL=1 -DFFX_HLSL_5_0=1 -compiler=fxc -DSPD_NO_WAVE_OPERATIONS)

file(GLOB SHADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/../shaders/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/../shaders/*.hlsl")

set(PASS_SHADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/../shaders/ffx_fsr2_tcr_autogen_pass.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/../shaders/ffx_fsr2_autogen_reactive_pass.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/../shaders/ffx_fsr2_accumulate_pass.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/../shaders/ffx_fsr2_compute_luminance_pyramid_pass.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/../shaders/ffx_fsr2_depth_clip_pass.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/../shaders/ffx_fsr2_lock_pass.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/../shaders/ffx_fsr2_reconstruct_previous_depth_pass.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/../shaders/ffx_fsr2_rcas_pass.hlsl)

file(GLOB_RECURSE DX11
    "${CMAKE_CURRENT_SOURCE_DIR}/../ffx_assert.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

if (FSR2_BUILD_AS_DLL)
    add_library(ffx_fsr2_api_dx11_${FSR2_PLATFORM_NAME} SHARED ${DX11})
    target_link_libraries(ffx_fsr2_api_dx11_${FSR2_PLATFORM_NAME} PRIVATE dxguid)
else()
    add_library(ffx_fsr2_api_dx11_${FSR2_PLATFORM_NAME} STATIC ${DX11})
endif()

target_include_directories(ffx_fsr2_api_dx11_${FSR2_PLATFORM_NAME} PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/../shaders/dx11)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/../shaders/dx11)

if (FSR2_AUTO_COMPILE_SHADERS)
    set(FFX_SC_DEPENDENT_TARGET ffx_fsr2_api_dx11_${FSR2_PLATFORM_NAME})
else()
    set(FFX_SC_DEPENDENT_TARGET ffx_fsr2_api_dx11_shaders_${FSR2_PLATFORM_NAME})
    add_custom_target(${FFX_SC_DEPENDENT_TARGET})
endif()

if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.20.0") 
cmake_policy(SET CMP0116 OLD)
endif()
get_filename_component(PASS_SHADER_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR}/../shaders/dx11 ABSOLUTE)
foreach(PASS_SHADER ${PASS_SHADERS})
    get_filename_component(PASS_SHADER_FILENAME ${PASS_SHADER} NAME_WE)
    get_filename_component(PASS_SHADER_TARGET ${PASS_SHADER} NAME_WLE)
    set(PERMUTATION_HEADER ${PASS_SHADER_OUTPUT_PATH}/${PASS_SHADER_TARGET}_permutations.h)
    set(16BIT_PERMUTATION_HEADER ${PASS_SHADER_OUTPUT_PATH}/${PASS_SHADER_TARGET}_16bit_permutations.h)

    # combine base and permutation args
    set(FFX_SC_ARGS ${FFX_SC_BASE_ARGS} ${FFX_SC_DX11_BASE_ARGS} ${FFX_SC_PERMUTATION_ARGS})
    # disable compiling optimization for rcas, as it run faster in that way
    if(${PASS_SHADER_FILENAME} STREQUAL ffx_fsr2_rcas_pass)
        set(FFX_SC_ARGS ${FFX_SC_ARGS} -Od)
    endif()

    if (USE_DEPFILE)
        add_custom_command(
            OUTPUT ${PERMUTATION_HEADER}
            COMMAND ${FFX_SC_EXECUTABLE} ${FFX_SC_ARGS} -name=${PASS_SHADER_FILENAME} -DFFX_HALF=0 -T cs_5_0 -I ${CMAKE_CURRENT_SOURCE_DIR}/../shaders -output=${PASS_SHADER_OUTPUT_PATH} ${PASS_SHADER}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            DEPENDS ${PASS_SHADER}
            DEPFILE ${PERMUTATION_HEADER}.d
        )
        list(APPEND PERMUTATION_OUTPUTS ${PERMUTATION_HEADER})

        # skip 16-bit permutations for the compute luminance pyramid pass
        if (NOT ${PASS_SHADER_FILENAME} STREQUAL "ffx_fsr2_compute_luminance_pyramid_pass")
            # 16-bit
            add_custom_command(
                OUTPUT ${16BIT_PERMUTATION_HEADER}
                COMMAND ${FFX_SC_EXECUTABLE} ${FFX_SC_ARGS} -name=${PASS_SHADER_FILENAME}_16bit -DFFX_HALF=1 -T cs_5_0 -I ${CMAKE_CURRENT_SOURCE_DIR}/../shaders -output=${PASS_SHADER_OUTPUT_PATH} ${PASS_SHADER}
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                DEPENDS ${PASS_SHADER}
                DEPFILE ${16BIT_PERMUTATION_HEADER}.d
            )
            list(APPEND PERMUTATION_OUTPUTS ${16BIT_PERMUTATION_HEADER})
        endif()
    else()
        add_custom_command(
            OUTPUT ${PERMUTATION_HEADER}
            COMMAND ${FFX_SC_EXECUTABLE} ${FFX_SC_ARGS} -name=${PASS_SHADER_FILENAME} -DFFX_HALF=0 -T cs_5_0 -I ${CMAKE_CURRENT_SOURCE_DIR}/../shaders -output=${PASS_SHADER_OUTPUT_PATH} ${PASS_SHADER}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            DEPENDS ${PASS_SHADER}
        )
        list(APPEND PERMUTATION_OUTPUTS ${PERMUTATION_HEADER})

        # skip 16-bit permutations for the compute luminance pyramid pass
        if (NOT ${PASS_SHADER_FILENAME} STREQUAL "ffx_fsr2_compute_luminance_pyramid_pass")
            # 16-bit
            add_custom_command(
                OUTPUT ${16BIT_PERMUTATION_HEADER}
                COMMAND ${FFX_SC_EXECUTABLE} ${FFX_SC_ARGS} -name=${PASS_SHADER_FILENAME}_16bit -DFFX_HALF=1 -T cs_5_0 -I ${CMAKE_CURRENT_SOURCE_DIR}/../shaders -output=${PASS_SHADER_OUTPUT_PATH} ${PASS_SHADER}
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                DEPENDS ${PASS_SHADER}
            )
            list(APPEND PERMUTATION_OUTPUTS ${16BIT_PERMUTATION_HEADER})
        endif()
    endif()
endforeach(PASS_SHADER)

add_custom_target(shader_permutations_dx11 DEPENDS ${PERMUTATION_OUTPUTS})
add_dependencies(${FFX_SC_DEPENDENT_TARGET} shader_permutations_dx11)

source_group("source"  FILES ${DX11})
source_group("shaders" FILES ${SHADERS})