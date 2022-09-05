// This file is part of the FidelityFX SDK.
//
// Copyright (c) 2022 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_samplerless_texture_functions : require

#define FSR2_BIND_SRV_INPUT_COLOR                     0
#define FSR2_BIND_UAV_SPD_GLOBAL_ATOMIC               1
#define FSR2_BIND_UAV_EXPOSURE_MIP_LUMA_CHANGE        2
#define FSR2_BIND_UAV_EXPOSURE_MIP_5                  3
#define FSR2_BIND_UAV_EXPOSURE                        4
#define FSR2_BIND_CB_FSR2                             5
#define FSR2_BIND_CB_SPD                              6

#include "ffx_fsr2_callbacks_glsl.h"
#include "ffx_fsr2_common.h"

#if defined(FSR2_BIND_CB_SPD)
	layout (set = 1, binding = FSR2_BIND_CB_SPD, std140) uniform cbSPD_t
	{
		uint mips;
		uint numWorkGroups;
		uvec2 workGroupOffset;
		uvec2 renderSize;
	} cbSPD;

	uint MipCount()
	{
		return cbSPD.mips;
	}

	uint NumWorkGroups()
	{
		return cbSPD.numWorkGroups;
	}

	uvec2 WorkGroupOffset()
	{
		return cbSPD.workGroupOffset;
	}

	uvec2 SPD_RenderSize()
	{
		return cbSPD.renderSize;
	}
#else
	uint MipCount()
	{
		return 0;
	}

	uint NumWorkGroups()
	{
		return 0;
	}

	uvec2 WorkGroupOffset()
	{
		return uvec2(0);
	}

	uvec2 SPD_RenderSize()
	{
		return uvec2(0);
	}
#endif

vec2 SPD_LoadExposureBuffer()
{
#if defined(FSR2_BIND_UAV_EXPOSURE)
	return imageLoad(rw_exposure, ivec2(0,0)).xy;
#else
	return vec2(0);
#endif
}

void SPD_SetExposureBuffer(vec2 value)
{
#if defined(FSR2_BIND_UAV_EXPOSURE)
	imageStore(rw_exposure, ivec2(0,0), vec4(value, 0.0f, 0.0f));
#endif
}

vec4 SPD_LoadMipmap5(ivec2 iPxPos)
{
#if defined(FSR2_BIND_UAV_EXPOSURE_MIP_5)
	return vec4(imageLoad(rw_img_mip_5, iPxPos).x, 0.0f, 0.0f, 0.0f);
#else 
    return vec4(0);
#endif
}

void SPD_SetMipmap(ivec2 iPxPos, uint slice, float value)
{
	switch (slice)
	{
#if defined(FSR2_BIND_UAV_EXPOSURE_MIP_LUMA_CHANGE)
	case FFX_FSR2_SHADING_CHANGE_MIP_LEVEL:
		imageStore(rw_img_mip_shading_change, iPxPos, vec4(value, 0.0f, 0.0f, 0.0f));
		break;
#endif
#if defined(FSR2_BIND_UAV_EXPOSURE_MIP_5)
	case 5:
		imageStore(rw_img_mip_5, iPxPos, vec4(value, 0.0f, 0.0f, 0.0f));
		break;
#endif
	default:
        // avoid flattened side effect
#if defined(FSR2_BIND_UAV_EXPOSURE_MIP_LUMA_CHANGE)
		imageStore(rw_img_mip_shading_change, iPxPos, vec4(imageLoad(rw_img_mip_shading_change, iPxPos).x, 0.0f, 0.0f, 0.0f));
#elif defined(FSR2_BIND_UAV_EXPOSURE_MIP_5)
		imageStore(rw_img_mip_5, iPxPos, vec4(imageLoad(rw_img_mip_5, iPxPos).x, 0.0f, 0.0f, 0.0f));
#endif
		break;
	}
}

void SPD_IncreaseAtomicCounter(inout uint spdCounter)
{
#if defined(FSR2_BIND_UAV_SPD_GLOBAL_ATOMIC)
	spdCounter = imageAtomicAdd(rw_spd_global_atomic, ivec2(0,0), 1);
#endif
}

void SPD_ResetAtomicCounter()
{
#if defined(FSR2_BIND_UAV_SPD_GLOBAL_ATOMIC)
	imageStore(rw_spd_global_atomic, ivec2(0,0), uvec4(0));
#endif
}

#include "ffx_fsr2_compute_luminance_pyramid.h"

#ifndef FFX_FSR2_THREAD_GROUP_WIDTH
#define FFX_FSR2_THREAD_GROUP_WIDTH 256
#endif // #ifndef FFX_FSR2_THREAD_GROUP_WIDTH
#ifndef FFX_FSR2_THREAD_GROUP_HEIGHT
#define FFX_FSR2_THREAD_GROUP_HEIGHT 1
#endif // #ifndef FFX_FSR2_THREAD_GROUP_HEIGHT
#ifndef FFX_FSR2_THREAD_GROUP_DEPTH
#define FFX_FSR2_THREAD_GROUP_DEPTH 1
#endif // #ifndef FFX_FSR2_THREAD_GROUP_DEPTH
#ifndef FFX_FSR2_NUM_THREADS
#define FFX_FSR2_NUM_THREADS layout (local_size_x = FFX_FSR2_THREAD_GROUP_WIDTH, local_size_y = FFX_FSR2_THREAD_GROUP_HEIGHT, local_size_z = FFX_FSR2_THREAD_GROUP_DEPTH) in;
#endif // #ifndef FFX_FSR2_NUM_THREADS

FFX_FSR2_NUM_THREADS
void main()
{
    ComputeAutoExposure(gl_WorkGroupID.xyz, gl_LocalInvocationIndex);
}