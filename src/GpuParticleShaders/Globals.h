//
// Copyright (c) 2019 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include "ShaderConstants.h"


#define FLOAT       float
#define FLOAT2      float2
#define FLOAT3      float3
#define FLOAT4      float4
#define FLOAT2X2    float2x2
#define UINT        uint
#define UINT2       uint2
#define UINT3       uint3
#define UINT4       uint4


// Per-frame constant buffer
[[vk::binding( 10, 0 )]] cbuffer PerFrameConstantBuffer : register( b0 )
{
	float4	g_StartColor[ NUM_EMITTERS ];
	float4	g_EndColor[ NUM_EMITTERS ];

	float4	g_EmitterLightingCenter[ NUM_EMITTERS ];

	matrix	g_mViewProjection;
	matrix	g_mView;
	matrix	g_mViewInv;
	matrix  g_mProjection;
	matrix  g_mProjectionInv;

	float4	g_EyePosition;
	float4	g_SunDirection;
	float4	g_SunColor;
	float4	g_AmbientColor;

	float4	g_SunDirectionVS;

	uint	g_ScreenWidth;
	uint	g_ScreenHeight;
    float   g_InvScreenWidth;
    float   g_InvScreenHeight;
	
	float	g_AlphaThreshold;
    float	g_ElapsedTime;
	float	g_CollisionThickness;
	int	    g_CollideParticles;

	int		g_ShowSleepingParticles;
	int		g_EnableSleepState;
	float	g_FrameTime;
    int     g_MaxParticles;

    uint    g_NumTilesX;
	uint    g_NumTilesY;
	uint    g_NumCoarseCullingTilesX;
	uint    g_NumCoarseCullingTilesY;

	uint    g_NumCullingTilesPerCoarseTileX;
	uint    g_NumCullingTilesPerCoarseTileY;
    uint    g_AlignedScreenWidth;
    uint    g_Pad1;
};





// Declare the global samplers
[[vk::binding( 12, 0 )]] SamplerState g_samWrapLinear		: register( s0 );
[[vk::binding( 13, 0 )]] SamplerState g_samClampLinear		: register( s1 );
[[vk::binding( 14, 0 )]] SamplerState g_samWrapPoint		: register( s2 );

