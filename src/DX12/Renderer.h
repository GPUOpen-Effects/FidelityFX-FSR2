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

#pragma once

#include "stdafx.h"

#include "base/GBuffer.h"
#include "PostProc/MagnifierPS.h"
#include "UpscaleContext.h"
#include "GPUFrameRateLimiter.h"

#include "../GpuParticles/ParticleSystem.h"
#include "../GpuParticleShaders/ShaderConstants.h"
#include "AnimatedTexture.h"

struct UIState;

// We are queuing (backBufferCount + 0.5) frames, so we need to triple buffer the resources that get modified each frame
static const int backBufferCount = 3;

#define USE_SHADOWMASK false

using namespace CAULDRON_DX12;

//
// Renderer class is responsible for rendering resources management and recording command buffers.
class Renderer
{
public:
    void OnCreate(Device* pDevice, SwapChain *pSwapChain, float FontSize, bool bInvertedDepth = false);
    void OnDestroy();

    void OnCreateWindowSizeDependentResources(SwapChain *pSwapChain, UIState* pState);
    void OnDestroyWindowSizeDependentResources();

    void OnUpdateDisplayDependentResources(SwapChain *pSwapChain);

    int LoadScene(GLTFCommon *pGLTFCommon, int Stage = 0);
    void UnloadScene();

    void AllocateShadowMaps(GLTFCommon* pGLTFCommon);

    const std::vector<TimeStamp>& GetTimingValues() const { return m_TimeStamps; }
    std::string& GetScreenshotFileName() { return m_pScreenShotName; }

    void OnRender(UIState* pState, const Camera& Cam, SwapChain* pSwapChain);

    void BuildDevUI(UIState* pState);

private:

    struct State
    {
        float                               frameTime = 0.0f;
        int                                 numEmitters = 0;
        IParticleSystem::EmitterParams      emitters[10] = {};
        int                                 flags = 0;
        IParticleSystem::ConstantData       constantData = {};
    };

    struct EmissionRate
    {
        float           m_ParticlesPerSecond = 0.0f;    // Number of particles to emit per second
        float           m_Accumulation = 0.0f;          // Running total of how many particles to emit over elapsed time
    };

    void ResetScene();
    void PopulateEmitters(bool playAnimations, int activeScene, float frameTime);

    Device                         *m_pDevice;

    uint32_t                        m_Width;
    uint32_t                        m_Height;
    D3D12_VIEWPORT                  m_Viewport;
    D3D12_RECT                      m_RectScissor;
    bool                            m_HasTAA = false;
    bool                            m_bInvertedDepth;

    // Initialize helper classes
    ResourceViewHeaps               m_ResourceViewHeaps;
    UploadHeap                      m_UploadHeap;
    DynamicBufferRing               m_ConstantBufferRing;
    StaticBufferPool                m_VidMemBufferPool;
    CommandListRing                 m_CommandListRing;
    GPUTimestamps                   m_GPUTimer;
    GPUFrameRateLimiter             m_GpuFrameRateLimiter;

    //gltf passes
    GltfPbrPass                    *m_GLTFPBR;
    GltfBBoxPass                   *m_GLTFBBox;
    GltfDepthPass                  *m_GLTFDepth;
    GLTFTexturesAndBuffers         *m_pGLTFTexturesAndBuffers;

    // effects
    Bloom                           m_Bloom;
    SkyDome                         m_SkyDome;
    DownSamplePS                    m_DownSample;
    SkyDomeProc                     m_SkyDomeProc;
    ToneMapping                     m_ToneMappingPS;
    ToneMappingCS                   m_ToneMappingCS;
    ColorConversionPS               m_ColorConversionPS;
    MagnifierPS                     m_MagnifierPS;

    // GPU Particle System
    State                           m_state = {};
    IParticleSystem*                m_pGPUParticleSystem = nullptr;
    EmissionRate                    m_EmissionRates[NUM_EMITTERS] = {};

    AnimatedTextures                m_AnimatedTextures = {};

    // TAA
    CBV_SRV_UAV                     m_UpscaleSRVs;
    UpscaleContext*					m_pUpscaleContext;

    // GUI
    ImGUI                           m_ImGUI;

    // Temporary render targets
    GBuffer                         m_GBuffer;
    GBufferRenderPass               m_RenderPassFullGBuffer;
    GBufferRenderPass               m_RenderPassJustDepthAndHdr;

    Texture*                        m_pGBufferHDRTexture = NULL;
    Texture                         m_OpaqueTexture;

    Texture                         m_MotionVectorsDepthMap;
    DSV                             m_MotionVectorsDepthMapDSV;
    CBV_SRV_UAV                     m_MotionVectorsDepthMapSRV;

#if USE_SHADOWMASK
    // shadow mask
    Texture                         m_ShadowMask;
    CBV_SRV_UAV                     m_ShadowMaskUAV;
    CBV_SRV_UAV                     m_ShadowMaskSRV;
    ShadowResolvePass               m_shadowResolve;
#endif

    // shadowmaps
    typedef struct {
        Texture     ShadowMap;
        uint32_t    ShadowIndex;
        uint32_t    ShadowResolution;
        uint32_t    LightIndex;
    } SceneShadowInfo;

    std::vector<SceneShadowInfo>    m_shadowMapPool;
    DSV                             m_ShadowMapPoolDSV;
    CBV_SRV_UAV                     m_ShadowMapPoolSRV;

    Texture							m_renderOutput;
    RTV								m_renderOutputRTV;
    Texture							m_displayOutput;
    CBV_SRV_UAV						m_displayOutputSRV;
    RTV								m_displayOutputRTV;
    CBV_SRV_UAV                     m_displayOutputUAV;

    // widgets
    Wireframe                       m_Wireframe;
    WireframeBox                    m_WireframeBox;

    std::vector<TimeStamp>          m_TimeStamps;

    // screen shot
    std::string                     m_pScreenShotName = "";
    SaveTexture                     m_SaveTexture;
    AsyncPool                       m_AsyncPool;
};
