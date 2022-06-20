// FidelityFX Super Resolution Sample
//
// Copyright (c) 2022 Advanced Micro Devices, Inc. All rights reserved.
//
// This file is part of the FidelityFX Super Resolution beta which is 
// released under the BETA SOFTWARE EVALUATION LICENSE AGREEMENT.
//
// See file LICENSE.txt for full license details.

Texture2D<float4>   r_in            : register(t0);
RWTexture2D<float4> rw_out          : register(u0);

cbuffer cbBlit0 : register(b0) {

    int         g_outChannelRed;
    int         g_outChannelGreen;
    int         g_outChannelBlue;
    float2      outChannelRedMinMax;
    float2      outChannelGreenMinMax;
    float2      outChannelBlueMinMax;
};

[numthreads(8, 8, 1)]
void CS(uint3 globalID : SV_DispatchThreadID)
{
    float4 srcColor = r_in[globalID.xy];

    // remap channels
    float4 dstColor = float4(srcColor[g_outChannelRed], srcColor[g_outChannelGreen], srcColor[g_outChannelBlue], 1.f);

    // apply offset and scale
    dstColor.rgb -= float3(outChannelRedMinMax.x, outChannelGreenMinMax.x, outChannelBlueMinMax.x);
    dstColor.rgb /= float3(outChannelRedMinMax.y - outChannelRedMinMax.x, outChannelGreenMinMax.y - outChannelGreenMinMax.x, outChannelBlueMinMax.y - outChannelBlueMinMax.x);

    rw_out[globalID.xy] = float4(dstColor);
}
