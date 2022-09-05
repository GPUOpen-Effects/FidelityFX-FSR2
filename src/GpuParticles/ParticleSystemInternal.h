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
#pragma once

#include "stdafx.h"
#include "../GpuParticleShaders/ShaderConstants.h"
#include "ParticleSystem.h"


// Helper function to align values
int align( int value, int alignment ) { return ( value + (alignment - 1) ) & ~(alignment - 1); }


// GPUParticle structure is split into two sections for better cache efficiency - could even be SOA but would require creating more vertex buffers.
struct GPUParticlePartA
{
    math::Vector4   m_params[ 2 ];
};

struct GPUParticlePartB
{
    math::Vector4   m_params[ 3 ];
};


struct SimulationConstantBuffer
{
    math::Vector4    m_StartColor[ NUM_EMITTERS ] = {};
    math::Vector4    m_EndColor[ NUM_EMITTERS ] = {};

    math::Vector4    m_EmitterLightingCenter[ NUM_EMITTERS ] = {};

    math::Matrix4    m_ViewProjection = {};
    math::Matrix4    m_View = {};
    math::Matrix4    m_ViewInv = {};
    math::Matrix4    m_ProjectionInv = {};

    math::Vector4    m_EyePosition = {};
    math::Vector4    m_SunDirection = {};

    UINT            m_ScreenWidth = 0;
    UINT            m_ScreenHeight = 0;
    float           m_ElapsedTime = 0.0f;
    float           m_CollisionThickness = 4.0f;

    int             m_CollideParticles = 0;
    int             m_ShowSleepingParticles = 0;
    int             m_EnableSleepState = 0;
    float           m_FrameTime = 0.0f;

    int             m_MaxParticles = 0;
    UINT            m_pad01 = 0;
    UINT            m_pad02 = 0;
    UINT            m_pad03 = 0;
};

struct EmitterConstantBuffer
{
    math::Vector4    m_EmitterPosition = {};
    math::Vector4    m_EmitterVelocity = {};
    math::Vector4    m_PositionVariance = {};

    int             m_MaxParticlesThisFrame = 0;
    float           m_ParticleLifeSpan = 0.0f;
    float           m_StartSize = 0.0f;
    float           m_EndSize = 0.0f;

    float           m_VelocityVariance = 0.0f;
    float           m_Mass = 0.0f;
    int             m_Index = 0;
    int             m_Streaks = 0;

    int             m_TextureIndex = 0;
    int             m_pads[ 3 ] = {};
};


// The rasterization path constant buffer
struct RenderingConstantBuffer
{
    math::Matrix4    m_Projection = {};
    math::Matrix4    m_ProjectionInv = {};
    math::Vector4    m_SunColor = {};
    math::Vector4    m_AmbientColor = {};
    math::Vector4    m_SunDirectionVS = {};
    UINT        m_ScreenWidth = 0;
    UINT        m_ScreenHeight = 0;
    UINT        m_pads[ 2 ] = {};
};

struct CullingConstantBuffer
{
    math::Matrix4    m_ProjectionInv = {};
    math::Matrix4    m_Projection = {};

    UINT        m_ScreenWidth = 0;
    UINT        m_ScreenHeight = 0;
    UINT        m_NumTilesX = 0;
    UINT        m_NumCoarseCullingTilesX = 0;

    UINT        m_NumCullingTilesPerCoarseTileX = 0;
    UINT        m_NumCullingTilesPerCoarseTileY = 0;
    UINT        m_pad01 = 0;
    UINT        m_pad02 = 0;
};

struct TiledRenderingConstantBuffer
{
    math::Matrix4    m_ProjectionInv = {};
    math::Vector4    m_SunColor = {};
    math::Vector4    m_AmbientColor = {};
    math::Vector4    m_SunDirectionVS = {};

    UINT        m_ScreenHeight = 0;
    float       m_InvScreenWidth = 0.0f;
    float       m_InvScreenHeight = 0.0f;
    float       m_AlphaThreshold = 0.97f;

    UINT        m_NumTilesX = 0;
    UINT        m_NumCoarseCullingTilesX = 0;
    UINT        m_NumCullingTilesPerCoarseTileX = 0;
    UINT        m_NumCullingTilesPerCoarseTileY = 0;

    UINT        m_AlignedScreenWidth = 0;
    UINT        m_pads[ 3 ] = {};
};

struct QuadConstantBuffer
{
    UINT m_AlignedScreenWidth;
    UINT m_pads[ 3 ];
};

// The maximum number of supported GPU particles
static const int g_maxParticles = 400*1024;
