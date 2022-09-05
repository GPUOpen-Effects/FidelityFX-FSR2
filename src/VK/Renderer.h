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
#include "AnimatedTexture.h"

#include "../GpuParticles/ParticleSystem.h"
#include "../GpuParticleShaders/ShaderConstants.h"

// We are queuing (backBufferCount + 0.5) frames, so we need to triple buffer the resources that get modified each frame
static const int backBufferCount = 3;

using namespace CAULDRON_VK;

struct UIState;

//
// Renderer class is responsible for rendering resources management and recording command buffers.
class Renderer
{
public:
    void OnCreate(Device *pDevice, SwapChain *pSwapChain, float FontSize, bool bInvertedDepth = false);
    void OnDestroy();

    void OnCreateWindowSizeDependentResources(SwapChain *pSwapChain, UIState* pState);
    void OnDestroyWindowSizeDependentResources();

    void OnUpdateDisplayDependentResources(SwapChain *pSwapChain);
    void OnUpdateLocalDimmingChangedResources(SwapChain *pSwapChain);

    int LoadScene(GLTFCommon *pGLTFCommon, int Stage = 0);
    void UnloadScene();

    void AllocateShadowMaps(GLTFCommon* pGLTFCommon);

    const std::vector<TimeStamp> &GetTimingValues() { return m_TimeStamps; }
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

    Device *m_pDevice;

    uint32_t                        m_Width;
    uint32_t                        m_Height;

    bool                            m_HasTAA = false;
    bool                            m_bInvertedDepth;

    // Initialize helper classes
    ResourceViewHeaps               m_ResourceViewHeaps;
    UploadHeap                      m_UploadHeap;
    DynamicBufferRing               m_ConstantBufferRing;
    StaticBufferPool                m_VidMemBufferPool;
    StaticBufferPool                m_SysMemBufferPool;
    CommandListRing                 m_CommandListRing;
    GPUTimestamps                   m_GPUTimer;
    GPUFrameRateLimiter             m_GpuFrameRateLimiter;

    // GLTF passes
    GltfPbrPass                    *m_GLTFPBR;
    GltfBBoxPass                   *m_GLTFBBox;
    GltfDepthPass                  *m_GLTFDepth;
    GLTFTexturesAndBuffers         *m_pGLTFTexturesAndBuffers;

    // Effects
    Bloom                           m_Bloom;
    SkyDome                         m_SkyDome;
    DownSamplePS                    m_DownSample;
    SkyDomeProc                     m_SkyDomeProc;
    ToneMapping                     m_ToneMappingPS;
    ToneMappingCS                   m_ToneMappingCS;
    ColorConversionPS               m_ColorConversionPS;
    MagnifierPS                     m_MagnifierPS;
    bool                            m_bMagResourceReInit = false;

    // GPU Particle System
    State                           m_state = {};
    IParticleSystem*                m_pGPUParticleSystem = nullptr;
    EmissionRate                    m_EmissionRates[NUM_EMITTERS] = {};

    AnimatedTextures                m_AnimatedTextures = {};

    // Upscale
    UpscaleContext*                 m_pUpscaleContext = nullptr;
    VkRenderPass                    m_RenderPassDisplayOutput;
    VkFramebuffer                   m_FramebufferDisplayOutput;

    // GUI
    ImGUI                           m_ImGUI;

    // GBuffer and render passes
    GBuffer                         m_GBuffer;
    GBufferRenderPass               m_RenderPassFullGBufferWithClear;
    GBufferRenderPass               m_RenderPassJustDepthAndHdr;
    GBufferRenderPass               m_RenderPassFullGBuffer;
    GBufferRenderPass               m_RenderPassFullGBufferNoDepthWrite;

    Texture                         m_OpaqueTexture;
    VkImageView                     m_OpaqueTextureSRV;

    // Shadow maps
    VkRenderPass                    m_Render_pass_shadow;

    typedef struct {
        Texture         ShadowMap;
        uint32_t        ShadowIndex;
        uint32_t        ShadowResolution;
        uint32_t        LightIndex;
        VkImageView     ShadowDSV;
        VkFramebuffer   ShadowFrameBuffer;
    } SceneShadowInfo;

    std::vector<SceneShadowInfo>    m_shadowMapPool;
    std::vector< VkImageView>       m_ShadowSRVPool;

    Texture m_displayOutput;
    VkImageView m_displayOutputSRV;

    // Widgets
    Wireframe                       m_Wireframe;
    WireframeBox                    m_WireframeBox;

    std::vector<TimeStamp>          m_TimeStamps;

    // Screen shot
    std::string                     m_pScreenShotName = "";
    // TODO: Implement!
    //SaveTexture                     m_SaveTexture;
    AsyncPool                       m_AsyncPool;
};

