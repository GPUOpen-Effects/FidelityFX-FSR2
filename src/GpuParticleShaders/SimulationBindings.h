//
// Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved.
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


// The particle buffers to fill with new particles
[[vk::binding( 0, 0 )]] RWStructuredBuffer<GPUParticlePartA>        g_ParticleBufferA           : register( u0 );
[[vk::binding( 1, 0 )]] RWStructuredBuffer<GPUParticlePartB>        g_ParticleBufferB           : register( u1 );

// The dead list, so any particles that are retired this frame can be added to this list. The first element is the number of dead particles
[[vk::binding( 2, 0 )]] RWStructuredBuffer<int>                     g_DeadList                  : register( u2 );

// The alive list which gets built in the similution.  The distances also get written out
[[vk::binding( 3, 0 )]] RWStructuredBuffer<int>                     g_IndexBuffer               : register( u3 );
[[vk::binding( 4, 0 )]] RWStructuredBuffer<float>                   g_DistanceBuffer            : register( u4 );

// The maximum radius in XY is calculated here and stored
[[vk::binding( 5, 0 )]] RWStructuredBuffer<float>                   g_MaxRadiusBuffer           : register( u5 );

// Viewspace particle positions are calculated here and stored
[[vk::binding( 6, 0 )]] RWStructuredBuffer<uint2>                   g_PackedViewSpacePositions  : register( u6 );

// The draw args for the ExecuteIndirect call needs to be filled in before the rasterization path is called, so do it here
struct IndirectCommand
{
#ifdef API_DX12
    uint2 uav;
#endif
    uint IndexCountPerInstance;
    uint InstanceCount;
    uint StartIndexLocation;
    int  BaseVertexLocation;
    uint StartInstanceLocation;
};
[[vk::binding( 7, 0 )]] RWStructuredBuffer<IndirectCommand>         g_DrawArgs                  : register( u7 );

[[vk::binding( 8, 0 )]] RWStructuredBuffer<uint>                    g_AliveParticleCount        : register( u8 );

// The opaque scene's depth buffer read as a texture
[[vk::binding(  9, 0 )]] Texture2D                                  g_DepthBuffer               : register( t0 );

// A texture filled with random values for generating some variance in our particles when we spawn them
[[vk::binding( 10, 0 )]] Texture2D                                  g_RandomBuffer              : register( t1 );


// Per-frame constant buffer
[[vk::binding( 11, 0 )]] cbuffer SimulationConstantBuffer : register( b0 )
{
    float4  g_StartColor[ NUM_EMITTERS ];
    float4  g_EndColor[ NUM_EMITTERS ];

    float4  g_EmitterLightingCenter[ NUM_EMITTERS ];

    matrix  g_mViewProjection;
    matrix  g_mView;
    matrix  g_mViewInv;
    matrix  g_mProjectionInv;

    float4  g_EyePosition;
    float4  g_SunDirection;

    uint    g_ScreenWidth;
    uint    g_ScreenHeight;
    float   g_ElapsedTime;
    float   g_CollisionThickness;

    int     g_CollideParticles;
    int     g_ShowSleepingParticles;
    int     g_EnableSleepState;
    float   g_FrameTime;

    int     g_MaxParticles;
    uint    g_Pad0;
    uint    g_Pad1;
    uint    g_Pad2;
};

[[vk::binding( 12, 0 )]] cbuffer EmitterConstantBuffer : register( b1 )
{
    float4  g_vEmitterPosition;
    float4  g_vEmitterVelocity;
    float4  g_PositionVariance;

    int     g_MaxParticlesThisFrame;
    float   g_ParticleLifeSpan;
    float   g_StartSize;
    float   g_EndSize;

    float   g_VelocityVariance;
    float   g_Mass;
    uint    g_EmitterIndex;
    uint    g_EmitterStreaks;

    uint    g_TextureIndex;
    uint    g_pads0;
    uint    g_pads1;
    uint    g_pads2;
};

[[vk::binding( 13, 0 )]] SamplerState g_samWrapPoint : register( s0 );
