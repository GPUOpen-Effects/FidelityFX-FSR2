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
#include "ffx_fsr2_resources.h"

#if defined(FFX_GPU)
#include "ffx_core.h"
#endif // #if defined(FFX_GPU)

#if defined(FFX_GPU)
#ifndef FFX_FSR2_PREFER_WAVE64
#define FFX_FSR2_PREFER_WAVE64
#endif // #if defined(FFX_GPU)

#if defined(FSR2_BIND_CB_FSR2)
	layout (set = 1, binding = FSR2_BIND_CB_FSR2, std140) uniform cbFSR2_t
	{
		FfxInt32x2   iRenderSize;
		FfxInt32x2   iDisplaySize;
		FfxInt32x2   uLumaMipDimensions;
		FfxInt32     uLumaMipLevelToUse;
		FfxInt32     uFrameIndex;
		FfxFloat32x2 fDisplaySizeRcp;
		FfxFloat32x2 fJitter;
		FfxFloat32x4 fDeviceToViewDepth;
		FfxFloat32x2 depthclip_uv_scale;
		FfxFloat32x2 postprocessed_lockstatus_uv_scale;
		FfxFloat32x2 reactive_mask_dim_rcp;
		FfxFloat32x2 MotionVectorScale;
		FfxFloat32x2 fDownscaleFactor;
		FfxFloat32   fPreExposure;
		FfxFloat32   fTanHalfFOV;
		FfxFloat32x2 fMotionVectorJitterCancellation;
		FfxFloat32   fJitterSequenceLength;
		FfxFloat32   fLockInitialLifetime;
		FfxFloat32   fLockTickDelta;
		FfxFloat32   fDeltaTime;
		FfxFloat32   fDynamicResChangeFactor;
		FfxFloat32   fLumaMipRcp;
	} cbFSR2;
#endif

FfxFloat32 LumaMipRcp()
{
	return cbFSR2.fLumaMipRcp;
}

FfxInt32x2 LumaMipDimensions()
{
	return cbFSR2.uLumaMipDimensions;
}

FfxInt32  LumaMipLevelToUse()
{
	return cbFSR2.uLumaMipLevelToUse;
}

FfxFloat32x2 DownscaleFactor()
{
	return cbFSR2.fDownscaleFactor;
}

FfxFloat32x2 Jitter()
{
	return cbFSR2.fJitter;
}

FfxFloat32x2 MotionVectorJitterCancellation()
{
	return cbFSR2.fMotionVectorJitterCancellation;
}

FfxInt32x2 RenderSize()
{
	return cbFSR2.iRenderSize;
}

FfxInt32x2 DisplaySize()
{
	return cbFSR2.iDisplaySize;
}

FfxFloat32x2 DisplaySizeRcp()
{
	return cbFSR2.fDisplaySizeRcp;
}

FfxFloat32 JitterSequenceLength()
{
	return cbFSR2.fJitterSequenceLength;
}

FfxFloat32 LockInitialLifetime()
{
	return cbFSR2.fLockInitialLifetime;
}

FfxFloat32 LockTickDelta()
{
	return cbFSR2.fLockTickDelta;
}

FfxFloat32 DeltaTime()
{
	return cbFSR2.fDeltaTime;
}

FfxFloat32 MaxAccumulationWeight()
{
    const FfxFloat32 averageLanczosWeightPerFrame = 0.74f; // Average lanczos weight for jitter accumulated samples

    return 12; //32.0f * averageLanczosWeightPerFrame;
}

FfxFloat32 DynamicResChangeFactor()
{
	return cbFSR2.fDynamicResChangeFactor;
}

FfxInt32 FrameIndex()
{
	return cbFSR2.uFrameIndex;
}

layout (set = 0, binding = 0) uniform sampler s_PointClamp;
layout (set = 0, binding = 1) uniform sampler s_LinearClamp;

// SRVs
#if defined(FSR2_BIND_SRV_INPUT_COLOR)
	layout (set = 1, binding = FSR2_BIND_SRV_INPUT_COLOR)                             uniform texture2D  r_input_color_jittered;
#endif
#if defined(FSR2_BIND_SRV_MOTION_VECTORS)
	layout (set = 1, binding = FSR2_BIND_SRV_MOTION_VECTORS)                          uniform texture2D  r_motion_vectors;
#endif
#if defined(FSR2_BIND_SRV_DEPTH)
	layout (set = 1, binding = FSR2_BIND_SRV_DEPTH)                                   uniform texture2D  r_depth;
#endif
#if defined(FSR2_BIND_SRV_EXPOSURE)
	layout (set = 1, binding = FSR2_BIND_SRV_EXPOSURE)                                uniform texture2D  r_exposure;
#endif
#if defined(FSR2_BIND_SRV_REACTIVE_MASK)
	layout (set = 1, binding = FSR2_BIND_SRV_REACTIVE_MASK)                           uniform texture2D  r_reactive_mask;
#endif
#if defined(FSR2_BIND_SRV_TRANSPARENCY_AND_COMPOSITION_MASK)
	layout (set = 1, binding = FSR2_BIND_SRV_TRANSPARENCY_AND_COMPOSITION_MASK)       uniform texture2D  r_transparency_and_composition_mask;
#endif
#if defined(FSR2_BIND_SRV_RECONSTRUCTED_PREV_NEAREST_DEPTH)
	layout (set = 1, binding = FSR2_BIND_SRV_RECONSTRUCTED_PREV_NEAREST_DEPTH)        uniform utexture2D r_reconstructed_previous_nearest_depth;
#endif
#if defined(FSR2_BIND_SRV_DILATED_MOTION_VECTORS)
	layout (set = 1, binding = FSR2_BIND_SRV_DILATED_MOTION_VECTORS)                  uniform texture2D  r_dilated_motion_vectors;
#endif
#if defined(FSR2_BIND_SRV_DILATED_DEPTH)
	layout (set = 1, binding = FSR2_BIND_SRV_DILATED_DEPTH)                           uniform texture2D  r_dilatedDepth;
#endif
#if defined(FSR2_BIND_SRV_INTERNAL_UPSCALED)
	layout (set = 1, binding = FSR2_BIND_SRV_INTERNAL_UPSCALED)                       uniform texture2D  r_internal_upscaled_color;
#endif
#if defined(FSR2_BIND_SRV_LOCK_STATUS)
	layout (set = 1, binding = FSR2_BIND_SRV_LOCK_STATUS)                             uniform texture2D  r_lock_status;
#endif
#if defined(FSR2_BIND_SRV_DEPTH_CLIP)
	layout (set = 1, binding = FSR2_BIND_SRV_DEPTH_CLIP)                              uniform texture2D  r_depth_clip;
#endif
#if defined(FSR2_BIND_SRV_PREPARED_INPUT_COLOR)
	layout (set = 1, binding = FSR2_BIND_SRV_PREPARED_INPUT_COLOR)                    uniform texture2D  r_prepared_input_color;
#endif
#if defined(FSR2_BIND_SRV_LUMA_HISTORY)
	layout (set = 1, binding = FSR2_BIND_SRV_LUMA_HISTORY)                            uniform texture2D  r_luma_history;
#endif
#if defined(FSR2_BIND_SRV_RCAS_INPUT)
	layout (set = 1, binding = FSR2_BIND_SRV_RCAS_INPUT)                              uniform texture2D  r_rcas_input;
#endif
#if defined(FSR2_BIND_SRV_LANCZOS_LUT)
	layout (set = 1, binding = FSR2_BIND_SRV_LANCZOS_LUT)                             uniform texture2D  r_lanczos_lut;
#endif
#if defined(FSR2_BIND_SRV_EXPOSURE_MIPS)
	layout (set = 1, binding = FSR2_BIND_SRV_EXPOSURE_MIPS)                           uniform texture2D  r_imgMips;
#endif
#if defined(FSR2_BIND_SRV_UPSCALE_MAXIMUM_BIAS_LUT)
	layout (set = 1, binding = FSR2_BIND_SRV_UPSCALE_MAXIMUM_BIAS_LUT)                uniform texture2D  r_upsample_maximum_bias_lut;
#endif
#if defined(FSR2_BIND_SRV_DILATED_REACTIVE_MASKS)
	layout (set = 1, binding = FSR2_BIND_SRV_DILATED_REACTIVE_MASKS)                  uniform texture2D  r_dilated_reactive_masks;
#endif			 

// UAV
#if defined FSR2_BIND_UAV_RECONSTRUCTED_PREV_NEAREST_DEPTH
	layout (set = 1, binding = FSR2_BIND_UAV_RECONSTRUCTED_PREV_NEAREST_DEPTH, r32ui) uniform uimage2D   rw_reconstructed_previous_nearest_depth;
#endif
#if defined FSR2_BIND_UAV_DILATED_MOTION_VECTORS
	layout (set = 1, binding = FSR2_BIND_UAV_DILATED_MOTION_VECTORS, rg32f)           uniform image2D    rw_dilated_motion_vectors;
#endif
#if defined FSR2_BIND_UAV_DILATED_DEPTH
	layout (set = 1, binding = FSR2_BIND_UAV_DILATED_DEPTH, r32f)                     uniform image2D    rw_dilatedDepth;
#endif
#if defined FSR2_BIND_UAV_INTERNAL_UPSCALED
	layout (set = 1, binding = FSR2_BIND_UAV_INTERNAL_UPSCALED, rgba32f)              uniform image2D    rw_internal_upscaled_color;
#endif
#if defined FSR2_BIND_UAV_LOCK_STATUS
	layout (set = 1, binding = FSR2_BIND_UAV_LOCK_STATUS, r11f_g11f_b10f)             uniform image2D    rw_lock_status;
#endif
#if defined FSR2_BIND_UAV_DEPTH_CLIP
	layout (set = 1, binding = FSR2_BIND_UAV_DEPTH_CLIP, r32f)                        uniform image2D    rw_depth_clip;
#endif
#if defined FSR2_BIND_UAV_PREPARED_INPUT_COLOR
	layout (set = 1, binding = FSR2_BIND_UAV_PREPARED_INPUT_COLOR, rgba16)            uniform image2D    rw_prepared_input_color;
#endif
#if defined FSR2_BIND_UAV_LUMA_HISTORY
	layout (set = 1, binding = FSR2_BIND_UAV_LUMA_HISTORY, rgba32f)                   uniform image2D    rw_luma_history;
#endif
#if defined FSR2_BIND_UAV_UPSCALED_OUTPUT
	layout (set = 1, binding = FSR2_BIND_UAV_UPSCALED_OUTPUT, rgba32f)                uniform image2D    rw_upscaled_output;
#endif
#if defined FSR2_BIND_UAV_EXPOSURE_MIP_LUMA_CHANGE
	layout (set = 1, binding = FSR2_BIND_UAV_EXPOSURE_MIP_LUMA_CHANGE, r32f) coherent uniform image2D    rw_img_mip_shading_change;
#endif
#if defined FSR2_BIND_UAV_EXPOSURE_MIP_5
	layout (set = 1, binding = FSR2_BIND_UAV_EXPOSURE_MIP_5, r32f)           coherent uniform image2D    rw_img_mip_5;
#endif
#if defined FSR2_BIND_UAV_DILATED_REACTIVE_MASKS
	layout (set = 1, binding = FSR2_BIND_UAV_DILATED_REACTIVE_MASKS, rg32f)           uniform image2D    rw_dilated_reactive_masks;
#endif 
#if defined FSR2_BIND_UAV_EXPOSURE 
	layout (set = 1, binding = FSR2_BIND_UAV_EXPOSURE, rg32f)                         uniform image2D    rw_exposure;
#endif 
#if defined FSR2_BIND_UAV_SPD_GLOBAL_ATOMIC 
	layout (set = 1, binding = FSR2_BIND_UAV_SPD_GLOBAL_ATOMIC, r32ui)       coherent uniform uimage2D   rw_spd_global_atomic;
#endif

FfxFloat32 LoadMipLuma(FfxInt32x2 iPxPos, FfxInt32 mipLevel)
{
#if defined(FSR2_BIND_SRV_EXPOSURE_MIPS)
	return texelFetch(r_imgMips, iPxPos, FfxInt32(mipLevel)).r;
#else
    return 0.f;
#endif
}


FfxFloat32 SampleMipLuma(FfxFloat32x2 fUV, FfxInt32 mipLevel)
{
#if defined(FSR2_BIND_SRV_EXPOSURE_MIPS)
	fUV *= cbFSR2.depthclip_uv_scale;
	return textureLod(sampler2D(r_imgMips, s_LinearClamp), fUV, FfxFloat32(mipLevel)).r;
#else
    return 0.f;
#endif
}

//
// a 0 0 0   x
// 0 b 0 0   y
// 0 0 c d   z
// 0 0 e 0   1
// 
// z' = (z*c+d)/(z*e)
// z' = (c/e) + d/(z*e)
// z' - (c/e) = d/(z*e)
// (z'e - c)/e = d/(z*e)
// e / (z'e - c) = (z*e)/d
// (e * d) / (z'e - c) = z*e
// z = d / (z'e - c)
FfxFloat32 ConvertFromDeviceDepthToViewSpace(FfxFloat32 fDeviceDepth)
{
	return -cbFSR2.fDeviceToViewDepth[2] / (fDeviceDepth * cbFSR2.fDeviceToViewDepth[1] - cbFSR2.fDeviceToViewDepth[0]);
}

FfxFloat32 LoadInputDepth(FfxInt32x2 iPxPos)
{
#if defined(FSR2_BIND_SRV_DEPTH)
	return texelFetch(r_depth, iPxPos, 0).r;
#else
	return 0.f;
#endif
}

FfxFloat32 LoadReactiveMask(FfxInt32x2 iPxPos)
{
#if defined(FSR2_BIND_SRV_REACTIVE_MASK) 
	return texelFetch(r_reactive_mask, FfxInt32x2(iPxPos), 0).r;
#else
	return 0.f;
#endif	
}

FfxFloat32x4 GatherReactiveMask(FfxInt32x2 iPxPos)
{
#if defined(FSR2_BIND_SRV_REACTIVE_MASK)
    return textureGather(sampler2D(r_reactive_mask, s_LinearClamp), FfxFloat32x2(iPxPos) * cbFSR2.reactive_mask_dim_rcp, 0);
#else
	return FfxFloat32x4(0.f);
#endif	
}

FfxFloat32 LoadTransparencyAndCompositionMask(FfxInt32x2 iPxPos)
{
#if defined(FSR2_BIND_SRV_TRANSPARENCY_AND_COMPOSITION_MASK)
	return texelFetch(r_transparency_and_composition_mask, iPxPos, 0).r;
#else 
	return 0.f;
#endif
}

FfxFloat32 SampleTransparencyAndCompositionMask(FfxFloat32x2 fUV)
{
#if defined(FSR2_BIND_SRV_TRANSPARENCY_AND_COMPOSITION_MASK)
	fUV *= cbFSR2.depthclip_uv_scale;
	return textureLod(sampler2D(r_transparency_and_composition_mask, s_LinearClamp), fUV, 0.0f).x;
#else
	return 0.f;
#endif
}

FfxFloat32 PreExposure()
{
	return cbFSR2.fPreExposure;
}

FfxFloat32x3 LoadInputColor(FfxInt32x2 iPxPos)
{
#if defined(FSR2_BIND_SRV_INPUT_COLOR)
	return texelFetch(r_input_color_jittered, iPxPos, 0).rgb / PreExposure();
#else
	return FfxFloat32x3(0.f);
#endif
}

FfxFloat32x3 LoadInputColorWithoutPreExposure(FfxInt32x2 iPxPos)
{
#if defined(FSR2_BIND_SRV_INPUT_COLOR)
	return texelFetch(r_input_color_jittered, iPxPos, 0).rgb;
#else
    return FfxFloat32x3(0.f);
#endif
}

FfxFloat32x3 LoadPreparedInputColor(FfxInt32x2 iPxPos)
{
#if defined(FSR2_BIND_SRV_PREPARED_INPUT_COLOR)
	return texelFetch(r_prepared_input_color, iPxPos, 0).rgb;
#else
	return FfxFloat32x3(0.f);
#endif
}

FfxFloat32 LoadPreparedInputColorLuma(FfxInt32x2 iPxPos)
{
#if defined(FSR2_BIND_SRV_PREPARED_INPUT_COLOR)
	return texelFetch(r_prepared_input_color, iPxPos, 0).a;
#else
	return 0.f;
#endif
}

FfxFloat32x2 LoadInputMotionVector(FfxInt32x2 iPxDilatedMotionVectorPos)
{
#if defined(FSR2_BIND_SRV_MOTION_VECTORS)
	FfxFloat32x2 fSrcMotionVector = texelFetch(r_motion_vectors, iPxDilatedMotionVectorPos, 0).xy;
#else
	FfxFloat32x2 fSrcMotionVector = FfxFloat32x2(0.f);
#endif

	FfxFloat32x2 fUvMotionVector = fSrcMotionVector * cbFSR2.MotionVectorScale;

#if FFX_FSR2_OPTION_JITTERED_MOTION_VECTORS
	fUvMotionVector -= cbFSR2.fMotionVectorJitterCancellation;
#endif

	return fUvMotionVector;
}

FfxFloat32x4 LoadHistory(FfxInt32x2 iPxHistory)
{
#if defined(FSR2_BIND_SRV_INTERNAL_UPSCALED)
	return texelFetch(r_internal_upscaled_color, iPxHistory, 0);
#else
	return FfxFloat32x4(0.0f);
#endif
}

FfxFloat32x4 LoadRwInternalUpscaledColorAndWeight(FfxInt32x2 iPxPos)
{
#if defined(FSR2_BIND_UAV_INTERNAL_UPSCALED)
	return imageLoad(rw_internal_upscaled_color, iPxPos);
#else
	return FfxFloat32x4(0.f);
#endif
}

void StoreLumaHistory(FfxInt32x2 iPxPos, FfxFloat32x4 fLumaHistory)
{
#if defined(FSR2_BIND_UAV_LUMA_HISTORY)
	imageStore(rw_luma_history, FfxInt32x2(iPxPos), fLumaHistory);
#endif
}

FfxFloat32x4 LoadRwLumaHistory(FfxInt32x2 iPxPos)
{
#if defined(FSR2_BIND_UAV_LUMA_HISTORY)
	return imageLoad(rw_luma_history, FfxInt32x2(iPxPos));
#else
	return FfxFloat32x4(1.f);
#endif
}

FfxFloat32 LoadLumaStabilityFactor(FfxInt32x2 iPxPos)
{
#if defined(FSR2_BIND_SRV_LUMA_HISTORY)
	return texelFetch(r_luma_history, FfxInt32x2(iPxPos), 0).w;
#else
    return 0.f;
#endif
}

FfxFloat32 SampleLumaStabilityFactor(FfxFloat32x2 fUV)
{
#if defined(FSR2_BIND_SRV_LUMA_HISTORY)
	fUV *= cbFSR2.depthclip_uv_scale;
	return textureLod(sampler2D(r_luma_history, s_LinearClamp), fUV, 0.0f).w;
#else
    return 0.f;
#endif
}

void StoreReprojectedHistory(FfxInt32x2 iPxHistory, FfxFloat32x4 fHistory)
{
#if defined(FSR2_BIND_UAV_INTERNAL_UPSCALED)
	imageStore(rw_internal_upscaled_color, iPxHistory, fHistory);
#endif
}

void StoreInternalColorAndWeight(FfxInt32x2 iPxPos, FfxFloat32x4 fColorAndWeight)
{
#if defined(FSR2_BIND_UAV_INTERNAL_UPSCALED)
	imageStore(rw_internal_upscaled_color, FfxInt32x2(iPxPos), fColorAndWeight);
#endif
}

void StoreUpscaledOutput(FfxInt32x2 iPxPos, FfxFloat32x3 fColor)
{
#if defined(FSR2_BIND_UAV_UPSCALED_OUTPUT)
    imageStore(rw_upscaled_output, FfxInt32x2(iPxPos), FfxFloat32x4(fColor * PreExposure(), 1.f));
#endif
}

FfxFloat32x3 LoadLockStatus(FfxInt32x2 iPxPos)
{
#if defined(FSR2_BIND_SRV_LOCK_STATUS)
	FfxFloat32x3 fLockStatus = texelFetch(r_lock_status, iPxPos, 0).rgb;

    fLockStatus[0] -= LockInitialLifetime() * 2.0f;

    return fLockStatus;
#else
	return FfxFloat32x3(0.f);
#endif
}

FfxFloat32x3 LoadRwLockStatus(FfxInt32x2 iPxPos)
{
#if defined(FSR2_BIND_UAV_LOCK_STATUS)
	FfxFloat32x3 fLockStatus = imageLoad(rw_lock_status, iPxPos).rgb;

	fLockStatus[0] -= LockInitialLifetime() * 2.0f;

    return fLockStatus;
#else
    return FfxFloat32x3(0.f);
#endif
}

void StoreLockStatus(FfxInt32x2 iPxPos, FfxFloat32x3 fLockstatus)
{
#if defined(FSR2_BIND_UAV_LOCK_STATUS)
	fLockstatus[0] += LockInitialLifetime() * 2.0f;

	imageStore(rw_lock_status, iPxPos, vec4(fLockstatus, 0.0f));
#endif
}

void StorePreparedInputColor(FFX_PARAMETER_IN FfxInt32x2 iPxPos, FFX_PARAMETER_IN FfxFloat32x4 fTonemapped)
{
#if defined(FSR2_BIND_UAV_PREPARED_INPUT_COLOR)
	imageStore(rw_prepared_input_color, iPxPos, fTonemapped);
#endif
}

FfxBoolean IsResponsivePixel(FfxInt32x2 iPxPos)
{
	return FFX_FALSE; //not supported in prototype
}

FfxFloat32 LoadDepthClip(FfxInt32x2 iPxPos)
{
#if defined(FSR2_BIND_SRV_DEPTH_CLIP)
	return texelFetch(r_depth_clip, iPxPos, 0).r;
#else
    return 0.f;
#endif
}

FfxFloat32 SampleDepthClip(FfxFloat32x2 fUV)
{
#if defined(FSR2_BIND_SRV_DEPTH_CLIP)
	fUV *= cbFSR2.depthclip_uv_scale;
	return textureLod(sampler2D(r_depth_clip, s_LinearClamp), fUV, 0.0f).r;
#else
    return 0.f;
#endif
}

FfxFloat32x3 SampleLockStatus(FfxFloat32x2 fUV)
{
#if defined(FSR2_BIND_SRV_LOCK_STATUS)
	fUV *= cbFSR2.postprocessed_lockstatus_uv_scale;
	FfxFloat32x3 fLockStatus = textureLod(sampler2D(r_lock_status, s_LinearClamp), fUV, 0.0f).rgb;
	fLockStatus[0] -= LockInitialLifetime() * 2.0f;
	return fLockStatus;
#else
    return FfxFloat32x3(0.f);
#endif
}

void StoreDepthClip(FfxInt32x2 iPxPos, FfxFloat32 fClip)
{
#if defined(FSR2_BIND_UAV_DEPTH_CLIP)
	imageStore(rw_depth_clip, iPxPos, vec4(fClip, 0.0f, 0.0f, 0.0f));
#endif
}

FfxFloat32 TanHalfFoV()
{
	return cbFSR2.fTanHalfFOV;
}

FfxFloat32 LoadSceneDepth(FfxInt32x2 iPxInput)
{
#if defined(FSR2_BIND_SRV_DEPTH)
	return texelFetch(r_depth, iPxInput, 0).r;
#else
    return 0.f;
#endif
}

FfxFloat32 LoadReconstructedPrevDepth(FfxInt32x2 iPxPos)
{
#if defined(FSR2_BIND_SRV_RECONSTRUCTED_PREV_NEAREST_DEPTH)
	return uintBitsToFloat(texelFetch(r_reconstructed_previous_nearest_depth, iPxPos, 0).r);
#else 
    return 0.f;
#endif
}

void StoreReconstructedDepth(FfxInt32x2 iPxSample, FfxFloat32 fDepth)
{
	FfxUInt32 uDepth = floatBitsToUint(fDepth);
#if defined(FSR2_BIND_UAV_RECONSTRUCTED_PREV_NEAREST_DEPTH)
	#if FFX_FSR2_OPTION_INVERTED_DEPTH
		imageAtomicMax(rw_reconstructed_previous_nearest_depth, iPxSample, uDepth);
	#else
		imageAtomicMin(rw_reconstructed_previous_nearest_depth, iPxSample, uDepth); // min for standard, max for inverted depth
	#endif
#endif
}

void SetReconstructedDepth(FfxInt32x2 iPxSample, FfxUInt32 uValue)
{
#if defined(FSR2_BIND_UAV_RECONSTRUCTED_PREV_NEAREST_DEPTH)
	imageStore(rw_reconstructed_previous_nearest_depth, iPxSample, uvec4(uValue, 0, 0, 0));
#endif
}

void StoreDilatedDepth(FFX_PARAMETER_IN FfxInt32x2 iPxPos, FFX_PARAMETER_IN FfxFloat32 fDepth)
{
#if defined(FSR2_BIND_UAV_DILATED_DEPTH)
	//FfxUInt32 uDepth = f32tof16(fDepth);
	imageStore(rw_dilatedDepth, iPxPos, vec4(fDepth, 0.0f, 0.0f, 0.0f));
#endif
}

void StoreDilatedMotionVector(FFX_PARAMETER_IN FfxInt32x2 iPxPos, FFX_PARAMETER_IN FfxFloat32x2 fMotionVector)
{
#if defined(FSR2_BIND_UAV_DILATED_MOTION_VECTORS) 
	imageStore(rw_dilated_motion_vectors, iPxPos, vec4(fMotionVector, 0.0f, 0.0f));
#endif
}

FfxFloat32x2 LoadDilatedMotionVector(FfxInt32x2 iPxInput)
{
#if defined(FSR2_BIND_SRV_DILATED_MOTION_VECTORS)
	return texelFetch(r_dilated_motion_vectors, iPxInput, 0).rg;
#else 
    return FfxFloat32x2(0.f);
#endif
}

FfxFloat32x2 SampleDilatedMotionVector(FfxFloat32x2 fUV)
{
#if defined(FSR2_BIND_SRV_DILATED_MOTION_VECTORS)
	fUV *= cbFSR2.depthclip_uv_scale; // TODO: assuming these are (RenderSize() / MaxRenderSize())
	return textureLod(sampler2D(r_dilated_motion_vectors, s_LinearClamp), fUV, 0.0f).rg;
#else
    return FfxFloat32x2(0.f);
#endif
}

FfxFloat32 LoadDilatedDepth(FfxInt32x2 iPxInput)
{
#if defined(FSR2_BIND_SRV_DILATED_DEPTH)
	return texelFetch(r_dilatedDepth, iPxInput, 0).r;
#else
    return 0.f;
#endif
}

FfxFloat32 Exposure()
{
	#if defined(FSR2_BIND_SRV_EXPOSURE)
		FfxFloat32 exposure = texelFetch(r_exposure, FfxInt32x2(0,0), 0).x;
	#else
		FfxFloat32 exposure = 1.f;
	#endif

	if (exposure == 0.0f) {
		exposure = 1.0f;
	}

	return exposure;
}

FfxFloat32 SampleLanczos2Weight(FfxFloat32 x)
{
#if defined(FSR2_BIND_SRV_LANCZOS_LUT)
	return textureLod(sampler2D(r_lanczos_lut, s_LinearClamp), FfxFloat32x2(x / 2.0f, 0.5f), 0.0f).x; 
#else
    return 0.f;
#endif
}

FfxFloat32 SampleUpsampleMaximumBias(FfxFloat32x2 uv)
{
#if defined(FSR2_BIND_SRV_UPSCALE_MAXIMUM_BIAS_LUT)
    // Stored as a SNORM, so make sure to multiply by 2 to retrieve the actual expected range.
    return FfxFloat32(2.0f) * FfxFloat32(textureLod(sampler2D(r_upsample_maximum_bias_lut, s_LinearClamp), abs(uv) * 2.0f, 0.0f).r);
#else
    return FfxFloat32(0.f);
#endif
}

FfxFloat32x2 SampleDilatedReactiveMasks(FfxFloat32x2 fUV)
{
#if defined(FSR2_BIND_SRV_DILATED_REACTIVE_MASKS)
	fUV *= cbFSR2.depthclip_uv_scale; // TODO: assuming these are (RenderSize() / MaxRenderSize())
	return textureLod(sampler2D(r_dilated_reactive_masks, s_LinearClamp), fUV, 0.0f).rg;
#else
	return FfxFloat32x2(0.f);
#endif
}

FfxFloat32x2 LoadDilatedReactiveMasks(FFX_PARAMETER_IN FfxInt32x2 iPxPos)
{
#if defined(FSR2_BIND_SRV_DILATED_REACTIVE_MASKS)
    return texelFetch(r_dilated_reactive_masks, iPxPos, 0).rg;
#else
    return FfxFloat32x2(0.f);
#endif
}

void StoreDilatedReactiveMasks(FFX_PARAMETER_IN FfxInt32x2 iPxPos, FFX_PARAMETER_IN FfxFloat32x2 fDilatedReactiveMasks)
{
#if defined(FSR2_BIND_UAV_DILATED_REACTIVE_MASKS)
    imageStore(rw_dilated_reactive_masks, iPxPos, vec4(fDilatedReactiveMasks, 0.0f, 0.0f));
#endif
}


#endif // #if defined(FFX_GPU)
