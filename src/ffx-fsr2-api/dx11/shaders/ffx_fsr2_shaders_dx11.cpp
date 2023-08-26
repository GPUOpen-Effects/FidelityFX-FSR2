// This file is part of the FidelityFX SDK.
//
// Copyright (c) 2023 Advanced Micro Devices, Inc. All rights reserved.
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

#include "ffx_fsr2_shaders_dx11.h"
#include "../../ffx_util.h"

#include <d3dcompiler.h>
#include <string>
#include <map>
#include <cassert>

#include "ffx_fsr2_tcr_autogen_pass_permutations.h"
#include "ffx_fsr2_autogen_reactive_pass_permutations.h"
#include "ffx_fsr2_accumulate_pass_permutations.h"
#include "ffx_fsr2_compute_luminance_pyramid_pass_permutations.h"
#include "ffx_fsr2_depth_clip_pass_permutations.h"
#include "ffx_fsr2_lock_pass_permutations.h"
#include "ffx_fsr2_reconstruct_previous_depth_pass_permutations.h"
#include "ffx_fsr2_rcas_pass_permutations.h"

#include "ffx_fsr2_tcr_autogen_pass_16bit_permutations.h"
#include "ffx_fsr2_autogen_reactive_pass_16bit_permutations.h"
#include "ffx_fsr2_accumulate_pass_16bit_permutations.h"
#include "ffx_fsr2_depth_clip_pass_16bit_permutations.h"
#include "ffx_fsr2_lock_pass_16bit_permutations.h"
#include "ffx_fsr2_reconstruct_previous_depth_pass_16bit_permutations.h"
#include "ffx_fsr2_rcas_pass_16bit_permutations.h"

#if defined(POPULATE_PERMUTATION_KEY)
#undef POPULATE_PERMUTATION_KEY
#endif // #if defined(POPULATE_PERMUTATION_KEY)
#define POPULATE_PERMUTATION_KEY(options, key)                                                                \
key.index = 0;                                                                                                \
key.FFX_FSR2_OPTION_REPROJECT_USE_LANCZOS_TYPE = FFX_CONTAINS_FLAG(options, FSR2_SHADER_PERMUTATION_USE_LANCZOS_TYPE);                     \
key.FFX_FSR2_OPTION_HDR_COLOR_INPUT = FFX_CONTAINS_FLAG(options, FSR2_SHADER_PERMUTATION_HDR_COLOR_INPUT);                 \
key.FFX_FSR2_OPTION_LOW_RESOLUTION_MOTION_VECTORS = FFX_CONTAINS_FLAG(options, FSR2_SHADER_PERMUTATION_LOW_RES_MOTION_VECTORS);   \
key.FFX_FSR2_OPTION_JITTERED_MOTION_VECTORS = FFX_CONTAINS_FLAG(options, FSR2_SHADER_PERMUTATION_JITTER_MOTION_VECTORS);   \
key.FFX_FSR2_OPTION_INVERTED_DEPTH = FFX_CONTAINS_FLAG(options, FSR2_SHADER_PERMUTATION_DEPTH_INVERTED);                   \
key.FFX_FSR2_OPTION_APPLY_SHARPENING = FFX_CONTAINS_FLAG(options, FSR2_SHADER_PERMUTATION_ENABLE_SHARPENING);

#if defined(POPULATE_SHADER_BLOB)
#undef POPULATE_SHADER_BLOB
#endif // #if defined(POPULATE_SHADER_BLOB)
#define POPULATE_SHADER_BLOB(info, index)  { info[index].blobData, info[index].blobSize, info[index].numUAVResources, info[index].numSRVResources, info[index].numCBVResources, info[index].uavResourceNames, info[index].uavResourceBindings, info[index].srvResourceNames, info[index].srvResourceBindings, info[index].cbvResourceNames, info[index].cbvResourceBindings }

static Fsr2ShaderBlobDX11 fsr2GetDepthClipPassPermutationBlobByIndex(uint32_t permutationOptions, bool is16bit)
{
    ffx_fsr2_depth_clip_pass_PermutationKey key;

    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (is16bit)
    {
        const int32_t tableIndex = g_ffx_fsr2_depth_clip_pass_16bit_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB(g_ffx_fsr2_depth_clip_pass_16bit_PermutationInfo, tableIndex);
    }
    else
    {
        const int32_t tableIndex = g_ffx_fsr2_depth_clip_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB(g_ffx_fsr2_depth_clip_pass_PermutationInfo, tableIndex);
    }
}

static Fsr2ShaderBlobDX11 fsr2GetReconstructPreviousDepthPassPermutationBlobByIndex(uint32_t permutationOptions, bool is16bit)
{
    ffx_fsr2_reconstruct_previous_depth_pass_PermutationKey key;

    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (is16bit)
    {
        const int32_t tableIndex = g_ffx_fsr2_reconstruct_previous_depth_pass_16bit_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB(g_ffx_fsr2_reconstruct_previous_depth_pass_16bit_PermutationInfo, tableIndex);
    }
    else
    {
        const int32_t tableIndex = g_ffx_fsr2_reconstruct_previous_depth_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB(g_ffx_fsr2_reconstruct_previous_depth_pass_PermutationInfo, tableIndex);
    }
}

static Fsr2ShaderBlobDX11 fsr2GetLockPassPermutationBlobByIndex(uint32_t permutationOptions, bool is16bit)
{
    ffx_fsr2_lock_pass_PermutationKey key;

    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (is16bit)
    {
        const int32_t tableIndex = g_ffx_fsr2_lock_pass_16bit_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB(g_ffx_fsr2_lock_pass_16bit_PermutationInfo, tableIndex);
    }
    else
    {
        const int32_t tableIndex = g_ffx_fsr2_lock_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB(g_ffx_fsr2_lock_pass_PermutationInfo, tableIndex);
    }
}

static Fsr2ShaderBlobDX11 fsr2GetAccumulatePassPermutationBlobByIndex(uint32_t permutationOptions, bool is16bit)
{
    ffx_fsr2_accumulate_pass_PermutationKey key;

    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (is16bit)
    {
        const int32_t tableIndex = g_ffx_fsr2_accumulate_pass_16bit_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB(g_ffx_fsr2_accumulate_pass_16bit_PermutationInfo, tableIndex);
    }
    else
    {
        const int32_t tableIndex = g_ffx_fsr2_accumulate_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB(g_ffx_fsr2_accumulate_pass_PermutationInfo, tableIndex);
    }
}

static Fsr2ShaderBlobDX11 fsr2GetRCASPassPermutationBlobByIndex(uint32_t permutationOptions, bool is16bit)
{
    ffx_fsr2_rcas_pass_PermutationKey key;

    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (is16bit)
    {
        const int32_t tableIndex = g_ffx_fsr2_rcas_pass_16bit_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB(g_ffx_fsr2_rcas_pass_16bit_PermutationInfo, tableIndex);
    }
    else
    {
        const int32_t tableIndex = g_ffx_fsr2_rcas_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB(g_ffx_fsr2_rcas_pass_PermutationInfo, tableIndex);
    }
}

static Fsr2ShaderBlobDX11 fsr2GetComputeLuminancePyramidPassPermutationBlobByIndex(uint32_t permutationOptions, bool is16bit)
{
    ffx_fsr2_compute_luminance_pyramid_pass_PermutationKey key;

    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    const int32_t tableIndex = g_ffx_fsr2_compute_luminance_pyramid_pass_IndirectionTable[key.index];
    return POPULATE_SHADER_BLOB(g_ffx_fsr2_compute_luminance_pyramid_pass_PermutationInfo, tableIndex);
}

static Fsr2ShaderBlobDX11 fsr2GetAutogenReactivePassPermutationBlobByIndex(uint32_t permutationOptions, bool is16bit) {

    ffx_fsr2_autogen_reactive_pass_PermutationKey key;

    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (is16bit) {

        const int32_t tableIndex = g_ffx_fsr2_autogen_reactive_pass_16bit_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB(g_ffx_fsr2_autogen_reactive_pass_16bit_PermutationInfo, tableIndex);
    }
    else {

        const int32_t tableIndex = g_ffx_fsr2_autogen_reactive_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB(g_ffx_fsr2_autogen_reactive_pass_PermutationInfo, tableIndex);
    }
}

static Fsr2ShaderBlobDX12 fsr2GetTcrAutogeneratePassPermutationBlobByIndex(uint32_t permutationOptions, bool is16bit) {

    ffx_fsr2_autogen_reactive_pass_PermutationKey key;

    POPULATE_PERMUTATION_KEY(permutationOptions, key);

    if (is16bit) {

        const int32_t tableIndex = g_ffx_fsr2_tcr_autogen_pass_16bit_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB(g_ffx_fsr2_tcr_autogen_pass_16bit_PermutationInfo, tableIndex);
    }
    else {

        const int32_t tableIndex = g_ffx_fsr2_tcr_autogen_pass_IndirectionTable[key.index];
        return POPULATE_SHADER_BLOB(g_ffx_fsr2_tcr_autogen_pass_PermutationInfo, tableIndex);
    }
}

Fsr2ShaderBlobDX11 fsr2GetPermutationBlobByIndexDX11(FfxFsr2Pass passId, uint32_t permutationOptions)
{
    bool is16bit = FFX_CONTAINS_FLAG(permutationOptions, FSR2_SHADER_PERMUTATION_ALLOW_FP16);

    switch (passId) {

    case FFX_FSR2_PASS_DEPTH_CLIP:
        return fsr2GetDepthClipPassPermutationBlobByIndex(permutationOptions, is16bit);
    case FFX_FSR2_PASS_RECONSTRUCT_PREVIOUS_DEPTH:
        return fsr2GetReconstructPreviousDepthPassPermutationBlobByIndex(permutationOptions, is16bit);
    case FFX_FSR2_PASS_LOCK:
        return fsr2GetLockPassPermutationBlobByIndex(permutationOptions, is16bit);
    case FFX_FSR2_PASS_ACCUMULATE:
    case FFX_FSR2_PASS_ACCUMULATE_SHARPEN:
        return fsr2GetAccumulatePassPermutationBlobByIndex(permutationOptions, is16bit);
    case FFX_FSR2_PASS_RCAS:
        return fsr2GetRCASPassPermutationBlobByIndex(permutationOptions, is16bit);
    case FFX_FSR2_PASS_COMPUTE_LUMINANCE_PYRAMID:
        return fsr2GetComputeLuminancePyramidPassPermutationBlobByIndex(permutationOptions, is16bit);
    case FFX_FSR2_PASS_GENERATE_REACTIVE:
        return fsr2GetAutogenReactivePassPermutationBlobByIndex(permutationOptions, is16bit);
    case FFX_FSR2_PASS_TCR_AUTOGENERATE:
        return fsr2GetTcrAutogeneratePassPermutationBlobByIndex(permutationOptions, is16bit);
    default:
        FFX_ASSERT_FAIL("Should never reach here.");
        break;
    }

    Fsr2ShaderBlobDX11 emptyBlob = {};
    return emptyBlob;
}
