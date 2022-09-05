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

// Implementation-agnostic particle system interface
struct IParticleSystem
{
    enum Flags
    {
        PF_Sort                     = 1 << 0,      // Sort the particles
        PF_DepthCull                = 1 << 1,      // Do per-tile depth buffer culling
        PF_Streaks                  = 1 << 2,      // Streak the particles based on velocity
        PF_Reactive                 = 1 << 3       // Particles also write to the reactive mask
    };

    // Per-emitter parameters
    struct EmitterParams
    {
        math::Vector4   m_Position = {};            // World position of the emitter
        math::Vector4   m_Velocity = {};            // Velocity of each particle from the emitter
        math::Vector4   m_PositionVariance = {};    // Variance in position of each particle
        int             m_NumToEmit = 0;            // Number of particles to emit this frame
        float           m_ParticleLifeSpan = 0.0f;  // How long the particles should live
        float           m_StartSize = 0.0f;         // Size of particles at spawn time
        float           m_EndSize = 0.0f;           // Size of particle when they reach retirement age
        float           m_Mass = 0.0f;              // Mass of particle
        float           m_VelocityVariance = 0.0f;  // Variance in velocity of each particle
        int             m_TextureIndex = 0;         // Index of the texture in the atlas
        bool            m_Streaks = false;          // Streak the particles in the direction of travel
    };

    struct ConstantData
    {
        math::Matrix4   m_ViewProjection = {};
        math::Matrix4   m_View = {};
        math::Matrix4   m_ViewInv = {};
        math::Matrix4   m_Projection = {};
        math::Matrix4   m_ProjectionInv = {};

        math::Vector4   m_StartColor[ 10 ] = {};
        math::Vector4   m_EndColor[ 10 ] = {};
        math::Vector4   m_EmitterLightingCenter[ 10 ] = {};

        math::Vector4   m_SunDirection = {};
        math::Vector4   m_SunColor = {};
        math::Vector4   m_AmbientColor = {};

        float           m_FrameTime = 0.0f;
    };

    // Create a GPU particle system. Add more factory functions to create other types of system eg CPU-updated system
    static IParticleSystem* CreateGPUSystem( const char* particleAtlas );

    virtual ~IParticleSystem() {}

#ifdef API_DX12
    virtual void Render( ID3D12GraphicsCommandList* pCommandList, DynamicBufferRing& constantBufferRing, int flags, const EmitterParams* pEmitters, int nNumEmitters, const ConstantData& constantData ) = 0;
    virtual void OnCreateDevice( Device &device, UploadHeap& uploadHeap, ResourceViewHeaps& heaps, StaticBufferPool& bufferPool, DynamicBufferRing& constantBufferRing ) = 0;
    virtual void OnResizedSwapChain( int width, int height, Texture& depthBuffer ) = 0;
#endif
#ifdef API_VULKAN
    virtual void Render( VkCommandBuffer commandBuffer, DynamicBufferRing& constantBufferRing, int contextFlags, const EmitterParams* pEmitters, int nNumEmitters, const ConstantData& constantData ) = 0;
    virtual void OnCreateDevice( Device &device, UploadHeap& uploadHeap, ResourceViewHeaps& heaps, StaticBufferPool& bufferPool, DynamicBufferRing& constantBufferRing, VkRenderPass renderPass ) = 0;
    virtual void OnResizedSwapChain( int width, int height, Texture& depthBuffer, VkFramebuffer frameBuffer ) = 0;
#endif

    virtual void OnReleasingSwapChain() = 0;
    virtual void OnDestroyDevice() = 0;

    // Completely resets the state of all particles. Handy for changing scenes etc
    virtual void Reset() = 0;
};
