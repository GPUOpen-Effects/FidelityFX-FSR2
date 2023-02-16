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

set(sources
    UpscaleContext.cpp
    UpscaleContext.h
    UpscaleContext_Spatial.cpp
    UpscaleContext_Spatial.h
    UpscaleContext_FSR2_API.cpp
    UpscaleContext_FSR2_API.h
    GPUFrameRateLimiter.cpp
    GPUFrameRateLimiter.h
    ShaderCompiler.cpp
    ShaderCompiler.h
    FSR2Sample.cpp
    FSR2Sample.h
    Renderer.cpp
    Renderer.h
    stdafx.cpp
    stdafx.h
    UI.cpp
    UI.h
    AnimatedTexture.cpp
    AnimatedTexture.h
    ../GpuParticles/ParticleHelpers.h
    ../GpuParticles/ParticleSystem.h
    ../GpuParticles/ParticleSystemInternal.h
    ../GpuParticles/dx12/GPUParticleSystem.cpp
    ../GpuParticles/dx12/ParallelSort.h
    ../GpuParticles/dx12/ParallelSort.cpp
    dpiawarescaling.manifest)

set(fsr1_shaders_src
    ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-fsr2-api/shaders/ffx_core.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-fsr2-api/shaders/ffx_common_types.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-fsr2-api/shaders/ffx_core_cpu.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-fsr2-api/shaders/ffx_core_gpu_common.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-fsr2-api/shaders/ffx_core_gpu_common_half.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-fsr2-api/shaders/ffx_core_portability.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-fsr2-api/shaders/ffx_core_hlsl.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-fsr2-api/shaders/ffx_fsr1.h)

set(spd_shaders_src
    ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-fsr2-api/shaders/ffx_core.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-fsr2-api/shaders/ffx_common_types.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-fsr2-api/shaders/ffx_core_cpu.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-fsr2-api/shaders/ffx_core_gpu_common.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-fsr2-api/shaders/ffx_core_gpu_common_half.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-fsr2-api/shaders/ffx_core_portability.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-fsr2-api/shaders/ffx_core_hlsl.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-fsr2-api/shaders/ffx_spd.h)

set(fsr2_shaders_src
    ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-fsr2-api/shaders/ffx_fsr2_accumulate.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-fsr2-api/shaders/ffx_fsr2_callbacks_hlsl.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-fsr2-api/shaders/ffx_fsr2_common.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-fsr2-api/shaders/ffx_fsr2_compute_luminance_pyramid.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-fsr2-api/shaders/ffx_fsr2_depth_clip.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-fsr2-api/shaders/ffx_core.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-fsr2-api/shaders/ffx_common_types.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-fsr2-api/shaders/ffx_core_cpu.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-fsr2-api/shaders/ffx_core_gpu_common.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-fsr2-api/shaders/ffx_core_gpu_common_half.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-fsr2-api/shaders/ffx_core_portability.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-fsr2-api/shaders/ffx_core_hlsl.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-fsr2-api/shaders/ffx_fsr1.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-fsr2-api/shaders/ffx_spd.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-fsr2-api/shaders/ffx_fsr2_common.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-fsr2-api/shaders/ffx_fsr2_lock.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-fsr2-api/shaders/ffx_fsr2_postprocess_lock_status.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-fsr2-api/shaders/ffx_fsr2_accumulate_pass.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-fsr2-api/shaders/ffx_fsr2_compute_luminance_pyramid_pass.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-fsr2-api/shaders/ffx_fsr2_depth_clip_pass.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-fsr2-api/shaders/ffx_fsr2_lock_pass.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-fsr2-api/shaders/ffx_fsr2_reconstruct_dilated_velocity_and_previous_depth.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-fsr2-api/shaders/ffx_fsr2_reconstruct_previous_depth_pass.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-fsr2-api/shaders/ffx_fsr2_reproject.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-fsr2-api/shaders/ffx_fsr2_resources.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-fsr2-api/shaders/ffx_fsr2_sample.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-fsr2-api/shaders/ffx_fsr2_upsample.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-fsr2-api/shaders/ffx_fsr2_rcas_pass.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-fsr2-api/shaders/ffx_fsr2_rcas.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-fsr2-api/shaders/ffx_fsr2_tcr_autogen.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-fsr2-api/shaders/ffx_fsr2_tcr_autogen_pass.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-fsr2-api/shaders/ffx_fsr2_autogen_reactive_pass.hlsl)

set(particle_shaders_src
    ${CMAKE_CURRENT_SOURCE_DIR}/../GpuParticleShaders/ParticleStructs.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../GpuParticleShaders/ParticleHelpers.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../GpuParticleShaders/fp16util.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../GpuParticleShaders/ParallelSortCS.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/../GpuParticleShaders/ParticleEmit.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/../GpuParticleShaders/ParticleRender.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/../GpuParticleShaders/ParticleSimulation.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/../GpuParticleShaders/ShaderConstants.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../GpuParticleShaders/SimulationBindings.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-parallelsort/FFX_ParallelSort.h)

set(sample_shaders_src
    ${CMAKE_CURRENT_SOURCE_DIR}/GPUFrameRateLimiter.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/AnimatedTexture.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/DebugBlit.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/UpscaleSpatial.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/FSRPass.hlsl)

set(APP_ICON_GPUOPEN "${CMAKE_CURRENT_SOURCE_DIR}/../common/GpuOpenIcon.rc")

source_group("sources" FILES ${sources})
source_group("spatial_shaders" FILES ${fsr1_shaders_src})
source_group("fsr2_shaders" FILES ${fsr2_shaders_src})
source_group("particle_shaders" FILES ${particle_shaders_src})
source_group("sample_shaders" FILES ${sample_shaders_src})

copyTargetCommand("${spd_shaders_src}" ${CMAKE_HOME_DIRECTORY}/bin/ShaderLibDX copied_dx12_shaders_spd_src)
add_dependencies(copied_dx12_shaders_spd_src Cauldron_DX12 copied_dx12_shaders_postproc_src)
copyTargetCommand("${fsr1_shaders_src}" ${CMAKE_HOME_DIRECTORY}/bin/ShaderLibDX copied_dx12_shaders_fsr1_src)
add_dependencies(copied_dx12_shaders_fsr1_src copied_dx12_shaders_spd_src)
copyTargetCommand("${fsr2_shaders_src}" ${CMAKE_HOME_DIRECTORY}/bin/ShaderLibDX copied_dx12_shaders_fsr2_src)
add_dependencies(copied_dx12_shaders_fsr2_src copied_dx12_shaders_fsr1_src)
copyCommand("${particle_shaders_src}" ${CMAKE_HOME_DIRECTORY}/bin/ShaderLibDX)
copyCommand("${sample_shaders_src}" ${CMAKE_HOME_DIRECTORY}/bin/ShaderLibDX)

add_executable(FSR2_Sample_DX12 WIN32 ${sources} ${fsr2_src} ${sample_shaders_src} ${fsr1_shaders_src} ${fsr2_shaders_src} ${particle_shaders_src} ${spd_shaders_src} ${common} ${APP_ICON_GPUOPEN})
target_compile_definitions(FSR2_Sample_DX12 PRIVATE USE_PIX=1 $<$<CONFIG:RelWithDebInfo>:FSR2_DEBUG_SHADERS=1>)
target_link_libraries(FSR2_Sample_DX12 LINK_PUBLIC FSR2_Sample_Common Cauldron_DX12 ImGUI amd_ags d3dcompiler D3D12 ffx_fsr2_api_x64 ffx_fsr2_api_dx12_x64)
target_include_directories(FSR2_Sample_DX12 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../ffx-fsr2-api ${CMAKE_CURRENT_SOURCE_DIR}/../../libs)
target_link_directories(FSR2_Sample_DX12 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../libs)
add_dependencies(FSR2_Sample_DX12 copied_dx12_shaders_fsr2_src)

set_target_properties(FSR2_Sample_DX12 PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_HOME_DIRECTORY}/bin" DEBUG_POSTFIX "d")
#set_source_files_properties(${sample_shaders_src} PROPERTIES VS_TOOL_OVERRIDE "Text")
