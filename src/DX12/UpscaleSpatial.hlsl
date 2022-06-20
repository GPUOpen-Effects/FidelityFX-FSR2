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

#define GROUP_SIZE  8

// These must match UpscaleType
#define UPSCALE_TYPE_POINT      0
#define UPSCALE_TYPE_BILINEAR   1
#define UPSCALE_TYPE_BICUBIC    2
#define UPSCALE_TYPE_FSR1       3
#define UPSCALE_TYPE_NATIVE     5


#if SAMPLE_EASU || SAMPLE_RCAS
[[vk::binding(0)]] cbuffer cb : register(b0)
{
    uint4 Const0;
    uint4 Const1;
    uint4 Const2;
    uint4 Const3;
    uint4 Sample;
};
#endif

#define FFX_GPU 1
#define FFX_HLSL 1

[[vk::binding(3)]] SamplerState    PointSampler		: register(s0);
[[vk::binding(4)]] SamplerState    LinearSampler	: register(s1);

#if SAMPLE_SLOW_FALLBACK
    #include "ffx_core.h"
    [[vk::binding(1)]] Texture2D ColorBuffer : register(t0);
    [[vk::binding(2)]] RWTexture2D<float4> OutputTexture   : register(u0);
    #if SAMPLE_EASU
        #define FFX_FSR_EASU_FLOAT
        FfxFloat32x4 FsrEasuRF(FfxFloat32x2 p) { FfxFloat32x4 res = ColorBuffer.GatherRed(LinearSampler, p, int2(0, 0)); return res; }
        FfxFloat32x4 FsrEasuGF(FfxFloat32x2 p) { FfxFloat32x4 res = ColorBuffer.GatherGreen(LinearSampler, p, int2(0, 0)); return res; }
        FfxFloat32x4 FsrEasuBF(FfxFloat32x2 p) { FfxFloat32x4 res = ColorBuffer.GatherBlue(LinearSampler, p, int2(0, 0)); return res; }
    #endif
    #if SAMPLE_RCAS
        #define FSR_RCAS_F
        FfxFloat32x4 FsrRcasLoadF(ASU2 p) { return ColorBuffer.Load(int3(ASU2(p), 0)); }
        void FsrRcasInputF(inout FfxFloat32 r, inout FfxFloat32 g, inout FfxFloat32 b) {}
    #endif
#else
    #define FFX_HALF 1
    #include "ffx_core.h"
    [[vk::binding(1)]] Texture2D<FfxFloat16x4> ColorBuffer : register(t0);
    [[vk::binding(2)]] RWTexture2D<FfxFloat16x4> OutputTexture : register(u0);
    #if SAMPLE_EASU
        #define FFX_FSR_EASU_HALF
        FfxFloat16x4 FsrEasuRH(FfxFloat32x2 p) { FfxFloat16x4 res = ColorBuffer.GatherRed(LinearSampler, p, int2(0, 0)); return res; }
        FfxFloat16x4 FsrEasuGH(FfxFloat32x2 p) { FfxFloat16x4 res = ColorBuffer.GatherGreen(LinearSampler, p, int2(0, 0)); return res; }
        FfxFloat16x4 FsrEasuBH(FfxFloat32x2 p) { FfxFloat16x4 res = ColorBuffer.GatherBlue(LinearSampler, p, int2(0, 0)); return res; }
    #endif
    #if SAMPLE_RCAS
        #define FSR_RCAS_H
        FfxFloat16x4 FsrRcasLoadH(FfxInt16x2  p) { return ColorBuffer.Load(FfxInt16x3(FfxInt16x2 (p), 0)); }
        void FsrRcasInputH(inout FfxFloat16  r, inout FfxFloat16  g, inout FfxFloat16  b) {}
    #endif
#endif

#include "ffx_fsr1.h"

/**********************************************************************
MIT License

Copyright(c) 2019 MJP

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files(the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
********************************************************************/
float4 SampleHistoryCatmullRom(in float2 uv, in float2 texelSize)
{
    // Source: https://gist.github.com/TheRealMJP/c83b8c0f46b63f3a88a5986f4fa982b1
    // License: https://gist.github.com/TheRealMJP/bc503b0b87b643d3505d41eab8b332ae

    // We're going to sample a a 4x4 grid of texels surrounding the target UV coordinate. We'll do this by rounding
    // down the sample location to get the exact center of our "starting" texel. The starting texel will be at
    // location [1, 1] in the grid, where [0, 0] is the top left corner.
    float2 samplePos = uv / texelSize;
    float2 texPos1 = floor(samplePos - 0.5f) + 0.5f;

    // Compute the fractional offset from our starting texel to our original sample location, which we'll
    // feed into the Catmull-Rom spline function to get our filter weights.
    float2 f = samplePos - texPos1;

    // Compute the Catmull-Rom weights using the fractional offset that we calculated earlier.
    // These equations are pre-expanded based on our knowledge of where the texels will be located,
    // which lets us avoid having to evaluate a piece-wise function.
    float2 w0 = f * (-0.5f + f * (1.0f - 0.5f * f));
    float2 w1 = 1.0f + f * f * (-2.5f + 1.5f * f);
    float2 w2 = f * (0.5f + f * (2.0f - 1.5f * f));
    float2 w3 = f * f * (-0.5f + 0.5f * f);

    // Work out weighting factors and sampling offsets that will let us use bilinear filtering to
    // simultaneously evaluate the middle 2 samples from the 4x4 grid.
    float2 w12 = w1 + w2;
    float2 offset12 = w2 / (w1 + w2);

    // Compute the final UV coordinates we'll use for sampling the texture
    float2 texPos0 = texPos1 - 1.0f;
    float2 texPos3 = texPos1 + 2.0f;
    float2 texPos12 = texPos1 + offset12;

    texPos0 *= texelSize;
    texPos3 *= texelSize;
    texPos12 *= texelSize;

    float4 result = 0.f;

    result += ColorBuffer.SampleLevel(LinearSampler, float2(texPos0.x, texPos0.y), 0.0f) * w0.x * w0.y;
    result += ColorBuffer.SampleLevel(LinearSampler, float2(texPos12.x, texPos0.y), 0.0f) * w12.x * w0.y;
    result += ColorBuffer.SampleLevel(LinearSampler, float2(texPos3.x, texPos0.y), 0.0f) * w3.x * w0.y;

    result += ColorBuffer.SampleLevel(LinearSampler, float2(texPos0.x, texPos12.y), 0.0f) * w0.x * w12.y;
    result += ColorBuffer.SampleLevel(LinearSampler, float2(texPos12.x, texPos12.y), 0.0f) * w12.x * w12.y;
    result += ColorBuffer.SampleLevel(LinearSampler, float2(texPos3.x, texPos12.y), 0.0f) * w3.x * w12.y;

    result += ColorBuffer.SampleLevel(LinearSampler, float2(texPos0.x, texPos3.y), 0.0f) * w0.x * w3.y;
    result += ColorBuffer.SampleLevel(LinearSampler, float2(texPos12.x, texPos3.y), 0.0f) * w12.x * w3.y;
    result += ColorBuffer.SampleLevel(LinearSampler, float2(texPos3.x, texPos3.y), 0.0f) * w3.x * w3.y;

    return max(result, 0.0f);
}

#if SAMPLE_EASU || SAMPLE_RCAS
    float4 ReinhardInverse(in float4 sdr)
    {
        return float4( sdr.xyz / max(1.0f - sdr.xyz, 1e-5f), sdr.w);
    }
    FfxFloat16x4 ReinhardInverse(in FfxFloat16x4 sdr)
    {
        return FfxFloat16x4( sdr.xyz / max(FfxFloat16 (1.0f) - sdr.xyz, FfxFloat16 (1e-5f)), sdr.w);
    }

    void CurrFilter(int2 pos)
    {
        const float2 texelSize = FfxFloat32x2(1.f, -1.f) * asfloat(Const1.zw);
        FfxFloat32x2 uv = (FfxFloat32x2(pos) * asfloat(Const0.xy) + asfloat(Const0.zw)) * asfloat(Const1.xy) + FfxFloat32x2(0.5, -0.5) * asfloat(Const1.zw);
        #if SAMPLE_SLOW_FALLBACK
            FfxFloat32x4 finalColor = 0.f;
        #else
            FfxFloat32x4 finalColor = 0.f;
        #endif

        #if (UPSCALE_TYPE == UPSCALE_TYPE_POINT) || (UPSCALE_TYPE == UPSCALE_TYPE_NATIVE)
            finalColor = ColorBuffer.SampleLevel(PointSampler, uv, 0.0);
        #elif UPSCALE_TYPE == UPSCALE_TYPE_BILINEAR
            finalColor = ColorBuffer.SampleLevel(LinearSampler, uv, 0.0);
        #elif UPSCALE_TYPE == UPSCALE_TYPE_BICUBIC
            finalColor = SampleHistoryCatmullRom(uv, texelSize);
        #elif UPSCALE_TYPE == UPSCALE_TYPE_FSR1
            #if SAMPLE_EASU
                #if SAMPLE_SLOW_FALLBACK
                    FsrEasuF(finalColor.xyz, pos, Const0, Const1, Const2, Const3);
                #else
                    FsrEasuH(finalColor.xyz, pos, Const0, Const1, Const2, Const3);
                #endif
            #endif
            #if SAMPLE_RCAS
                #if SAMPLE_SLOW_FALLBACK
                    FsrRcasF(finalColor.r, finalColor.g, finalColor.b, pos, Const0);
                #else
                    FsrRcasH(finalColor.r, finalColor.g, finalColor.b, pos, Const0);
                #endif
                if (Sample.x == 1)
                    finalColor.rgb *= finalColor.rgb;
            #endif
            finalColor.a = 1;
        #endif

        #if DO_INVREINHARD
            OutputTexture[pos] = ReinhardInverse(finalColor);
        #else
            OutputTexture[pos] = finalColor;
        #endif
    }


    [numthreads(WIDTH, HEIGHT, DEPTH)]
    void upscalePassCS(uint3 LocalThreadId : SV_GroupThreadID, uint3 WorkGroupId : SV_GroupID, uint3 Dtid : SV_DispatchThreadID)
    {
        // Do remapping of local xy in workgroup for a more PS-like swizzle pattern.
        FfxUInt32x2 gxy = ffxRemapForQuad(LocalThreadId.x) + FfxUInt32x2(WorkGroupId.x << 4u, WorkGroupId.y << 4u);
        CurrFilter(gxy);
        gxy.x += 8u;
        CurrFilter(gxy);
        gxy.y += 8u;
        CurrFilter(gxy);
        gxy.x -= 8u;
        CurrFilter(gxy);
    }
#else
    [numthreads(WIDTH, HEIGHT, DEPTH)]
    void BlitCS(uint3 globalID : SV_DispatchThreadID)
    {
        float4 srcColor = ColorBuffer[globalID.xy];

        // remap channels
        float4 dstColor = float4(srcColor[g_outChannelRed], srcColor[g_outChannelGreen], srcColor[g_outChannelBlue], 1.f);

        // apply offset and scale
        dstColor.rgb -= float3(outChannelRedMinMax.x, outChannelGreenMinMax.x, outChannelBlueMinMax.x);
        dstColor.rgb /= float3(outChannelRedMinMax.y - outChannelRedMinMax.x, outChannelGreenMinMax.y - outChannelGreenMinMax.x, outChannelBlueMinMax.y - outChannelBlueMinMax.x);


        #if FFX_HALF == 1
            OutputTexture[globalID.xy] = FfxFloat16x4(dstColor);
        #else
            OutputTexture[globalID.xy] = float4(dstColor);
        #endif
    }
#endif