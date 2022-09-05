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

#include "Renderer.h"
#include "UI.h"

//--------------------------------------------------------------------------------------
//
// OnCreate
//
//--------------------------------------------------------------------------------------
void Renderer::OnCreate(Device* pDevice, SwapChain *pSwapChain, float FontSize, bool bInvertedDepth)
{
    m_pDevice = pDevice;
    m_bInvertedDepth = bInvertedDepth;

    // Initialize helpers

    // Create all the heaps for the resources views
    const uint32_t cbvDescriptorCount = 4000;
    const uint32_t srvDescriptorCount = 8000;
    const uint32_t uavDescriptorCount = 100;
    const uint32_t dsvDescriptorCount = 100;
    const uint32_t rtvDescriptorCount = 100;
    const uint32_t samplerDescriptorCount = 20;
    m_ResourceViewHeaps.OnCreate(pDevice, cbvDescriptorCount, srvDescriptorCount, uavDescriptorCount, dsvDescriptorCount, rtvDescriptorCount, samplerDescriptorCount);

    // Create a commandlist ring for the Direct queue
    uint32_t commandListsPerBackBuffer = 8;
    m_CommandListRing.OnCreate(pDevice, backBufferCount, commandListsPerBackBuffer, pDevice->GetGraphicsQueue()->GetDesc());

    // Create a 'dynamic' constant buffer
    const uint32_t constantBuffersMemSize = 200 * 1024 * 1024;
    m_ConstantBufferRing.OnCreate(pDevice, backBufferCount, constantBuffersMemSize, &m_ResourceViewHeaps);

    // Create a 'static' pool for vertices, indices and constant buffers
    const uint32_t staticGeometryMemSize = (5 * 128) * 1024 * 1024;
    m_VidMemBufferPool.OnCreate(pDevice, staticGeometryMemSize, true, "StaticGeom");

    // initialize the GPU time stamps module
    m_GPUTimer.OnCreate(pDevice, backBufferCount);

    // Quick helper to upload resources, it has it's own commandList and uses suballocation.
    const uint32_t uploadHeapMemSize = 1000 * 1024 * 1024;
    m_UploadHeap.OnCreate(pDevice, uploadHeapMemSize);    // initialize an upload heap (uses suballocation for faster results)

    // Create GBuffer and render passes
    {
        {
            m_GBuffer.OnCreate(
                pDevice,
                &m_ResourceViewHeaps,
                {
                    { GBUFFER_DEPTH, DXGI_FORMAT_D32_FLOAT},
                    { GBUFFER_FORWARD, DXGI_FORMAT_R16G16B16A16_FLOAT},
                    { GBUFFER_MOTION_VECTORS, DXGI_FORMAT_R16G16_FLOAT},
                    { GBUFFER_UPSCALEREACTIVE, DXGI_FORMAT_R8_UNORM},
                    { GBUFFER_UPSCALE_TRANSPARENCY_AND_COMPOSITION, DXGI_FORMAT_R8_UNORM},
                },
                1,
                bInvertedDepth ? GBUFFER_INVERTED_DEPTH : GBUFFER_NONE
                );

            GBufferFlags fullGBuffer = GBUFFER_DEPTH | GBUFFER_FORWARD | GBUFFER_MOTION_VECTORS | GBUFFER_UPSCALEREACTIVE | GBUFFER_UPSCALE_TRANSPARENCY_AND_COMPOSITION;
            if (bInvertedDepth) fullGBuffer |= GBUFFER_INVERTED_DEPTH;
            m_RenderPassFullGBuffer.OnCreate(&m_GBuffer, fullGBuffer);

            GBufferFlags depthAndHdr = GBUFFER_DEPTH | GBUFFER_FORWARD;
            if (bInvertedDepth) depthAndHdr |= GBUFFER_INVERTED_DEPTH;
            m_RenderPassJustDepthAndHdr.OnCreate(&m_GBuffer, depthAndHdr);
        }
    }

#if USE_SHADOWMASK    
    m_shadowResolve.OnCreate(m_pDevice, &m_ResourceViewHeaps, &m_ConstantBufferRing);

    // Create the shadow mask descriptors
    m_ResourceViewHeaps.AllocCBV_SRV_UAVDescriptor(1, &m_ShadowMaskUAV);
    m_ResourceViewHeaps.AllocCBV_SRV_UAVDescriptor(1, &m_ShadowMaskSRV);
#endif

    m_ResourceViewHeaps.AllocCBV_SRV_UAVDescriptor(1, &m_displayOutputSRV);
    m_ResourceViewHeaps.AllocCBV_SRV_UAVDescriptor(1, &m_displayOutputUAV);
    m_ResourceViewHeaps.AllocRTVDescriptor(1, &m_renderOutputRTV);
    m_ResourceViewHeaps.AllocRTVDescriptor(1, &m_displayOutputRTV);

    m_SkyDome.OnCreate(pDevice, &m_UploadHeap, &m_ResourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, "..\\media\\cauldron-media\\envmaps\\papermill\\diffuse.dds", "..\\media\\cauldron-media\\envmaps\\papermill\\specular.dds", DXGI_FORMAT_R16G16B16A16_FLOAT, 4, m_bInvertedDepth);
    m_SkyDomeProc.OnCreate(pDevice, &m_ResourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, m_bInvertedDepth);
    m_Wireframe.OnCreate(pDevice, &m_ResourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, m_bInvertedDepth);
    m_WireframeBox.OnCreate(pDevice, &m_ResourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool);
    m_DownSample.OnCreate(pDevice, &m_ResourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, DXGI_FORMAT_R16G16B16A16_FLOAT);
    m_Bloom.OnCreate(pDevice, &m_ResourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, DXGI_FORMAT_R16G16B16A16_FLOAT);

    // Create tonemapping pass
    m_ToneMappingPS.OnCreate(pDevice, &m_ResourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, pSwapChain->GetFormat());
    m_ToneMappingCS.OnCreate(pDevice, &m_ResourceViewHeaps, &m_ConstantBufferRing);
    m_ColorConversionPS.OnCreate(pDevice, &m_ResourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, pSwapChain->GetFormat());

    // Initialize UI rendering resources
    m_ImGUI.OnCreate(pDevice, &m_UploadHeap, &m_ResourceViewHeaps, &m_ConstantBufferRing, pSwapChain->GetFormat(), FontSize);

    // Make sure upload heap has finished uploading before continuing
    m_VidMemBufferPool.UploadData(m_UploadHeap.GetCommandList());
    m_UploadHeap.FlushAndFinish();

    // TAA
    m_ResourceViewHeaps.AllocCBV_SRV_UAVDescriptor(3, &m_UpscaleSRVs);

    m_pGPUParticleSystem = IParticleSystem::CreateGPUSystem("..\\media\\atlas.dds");
    m_pGPUParticleSystem->OnCreateDevice(*pDevice, m_UploadHeap, m_ResourceViewHeaps, m_VidMemBufferPool, m_ConstantBufferRing);

    m_GpuFrameRateLimiter.OnCreate(pDevice, &m_ResourceViewHeaps);

    // needs to be completely reinitialized, as the format potentially changes
    bool hdr = (pSwapChain->GetDisplayMode() != DISPLAYMODE_SDR);
    DXGI_FORMAT mFormat = (hdr ? m_pGBufferHDRTexture->GetFormat() : DXGI_FORMAT_R8G8B8A8_UNORM);
    m_MagnifierPS.OnCreate(m_pDevice, &m_ResourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, mFormat);

    m_AnimatedTextures.OnCreate( *pDevice, m_UploadHeap, m_VidMemBufferPool, m_ResourceViewHeaps, m_ConstantBufferRing );

    ResetScene();
}

//--------------------------------------------------------------------------------------
//
// OnDestroy
//
//--------------------------------------------------------------------------------------
void Renderer::OnDestroy()
{
    m_AnimatedTextures.OnDestroy();
    m_GpuFrameRateLimiter.OnDestroy();

    m_pGPUParticleSystem->OnDestroyDevice();
    delete m_pGPUParticleSystem;
    m_pGPUParticleSystem = nullptr;

    m_AsyncPool.Flush();

    m_ImGUI.OnDestroy();
    m_ColorConversionPS.OnDestroy();
    m_ToneMappingCS.OnDestroy();
    m_ToneMappingPS.OnDestroy();
    m_Bloom.OnDestroy();
    m_DownSample.OnDestroy();
    m_WireframeBox.OnDestroy();
    m_Wireframe.OnDestroy();
    m_SkyDomeProc.OnDestroy();
    m_SkyDome.OnDestroy();
#if USE_SHADOWMASK
    m_shadowResolve.OnDestroy();
#endif
    m_GBuffer.OnDestroy();

    m_UploadHeap.OnDestroy();
    m_GPUTimer.OnDestroy();
    m_VidMemBufferPool.OnDestroy();
    m_ConstantBufferRing.OnDestroy();
    m_ResourceViewHeaps.OnDestroy();
    m_CommandListRing.OnDestroy();

    m_pUpscaleContext->OnDestroy();
    delete m_pUpscaleContext;
    m_pUpscaleContext = NULL;

    m_MagnifierPS.OnDestroy();
}

//--------------------------------------------------------------------------------------
//
// OnCreateWindowSizeDependentResources
//
//--------------------------------------------------------------------------------------
void Renderer::OnCreateWindowSizeDependentResources(SwapChain *pSwapChain, UIState* pState)
{
    m_Width = pState->renderWidth;
    m_Height = pState->renderHeight;

    // Set the viewport & scissors rect
    m_Viewport = { 0.0f, 0.0f, static_cast<float>(m_Width), static_cast<float>(m_Height), 0.0f, 1.0f };
    m_RectScissor = { 0, 0, (LONG)m_Width, (LONG)m_Height };

#if USE_SHADOWMASK
    // Create shadow mask
    //
    m_ShadowMask.Init(m_pDevice, "shadowbuffer", &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, Width, Height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, NULL);
    m_ShadowMask.CreateUAV(0, &m_ShadowMaskUAV);
    m_ShadowMask.CreateSRV(0, &m_ShadowMaskSRV);
#endif

    // Create GBuffer
    //
    m_GBuffer.OnCreateWindowSizeDependentResources(pSwapChain, m_Width, m_Height);
    m_RenderPassFullGBuffer.OnCreateWindowSizeDependentResources(m_Width, m_Height);
    m_RenderPassJustDepthAndHdr.OnCreateWindowSizeDependentResources(m_Width, m_Height);

    {
        m_GBuffer.m_HDR.CreateSRV(0, &m_UpscaleSRVs);
        m_GBuffer.m_DepthBuffer.CreateSRV(1, &m_UpscaleSRVs);
        m_GBuffer.m_MotionVectors.CreateSRV(2, &m_UpscaleSRVs);

        m_pGBufferHDRTexture = &m_GBuffer.m_HDR;
    }

    {
        CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(m_pGBufferHDRTexture->GetFormat(), m_pGBufferHDRTexture->GetWidth(), m_pGBufferHDRTexture->GetHeight(), 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        m_OpaqueTexture.Init(m_pDevice, "OpaqueBuffer", &desc, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, NULL);
    }

    // update bloom and downscaling effect
    //
    m_DownSample.OnCreateWindowSizeDependentResources(m_Width, m_Height, m_pGBufferHDRTexture, 5); //downsample the HDR texture 5 times
    m_Bloom.OnCreateWindowSizeDependentResources(m_Width / 2, m_Height / 2, m_DownSample.GetTexture(), 5, m_pGBufferHDRTexture);

    // update bloom and downscaling effect
    //
    bool renderNative = (pState->m_nUpscaleType == UPSCALE_TYPE_NATIVE);
    bool hdr = (pSwapChain->GetDisplayMode() != DISPLAYMODE_SDR);
    DXGI_FORMAT dFormat = (hdr ? m_pGBufferHDRTexture->GetFormat() : DXGI_FORMAT_R8G8B8A8_UNORM);
    DXGI_FORMAT rFormat = (hdr ? m_pGBufferHDRTexture->GetFormat() : pSwapChain->GetFormat());
    // Update pipelines in case the format of the RTs changed (this happens when going HDR)
    m_ColorConversionPS.UpdatePipelines(pSwapChain->GetFormat(), pSwapChain->GetDisplayMode());
    m_ToneMappingPS.UpdatePipelines(renderNative ? (hdr ? dFormat : DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) : rFormat);
    m_ImGUI.UpdatePipeline(rFormat);

    FLOAT cc[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    m_renderOutput.InitRenderTarget(m_pDevice, "RenderOutput", &CD3DX12_RESOURCE_DESC::Tex2D(rFormat, pState->renderWidth, pState->renderHeight, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, cc);
    m_renderOutput.CreateRTV(0, &m_renderOutputRTV);
    m_displayOutput.InitRenderTarget(m_pDevice, "DisplayOutput", &CD3DX12_RESOURCE_DESC::Tex2D(dFormat, pState->displayWidth, pState->displayHeight, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET), D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_displayOutput.CreateSRV(0, &m_displayOutputSRV);
    m_displayOutput.CreateUAV(0, &m_displayOutputUAV);
    if (hdr)
        m_displayOutput.CreateRTV(0, &m_displayOutputRTV, -1, -1, -1);
    else
        m_displayOutput.CreateRTV(0, &m_displayOutputRTV, -1, -1, -1);
    
    m_MagnifierPS.OnCreateWindowSizeDependentResources(&m_displayOutput);

    m_pGPUParticleSystem->OnResizedSwapChain(pState->renderWidth, pState->renderHeight, m_GBuffer.m_DepthBuffer);

    // Lazy Upscale context generation: 
    if ((m_pUpscaleContext == NULL) || (pState->m_nUpscaleType != m_pUpscaleContext->Type()))
    {
        if (m_pUpscaleContext)
        {
            m_pUpscaleContext->OnDestroy();
            delete m_pUpscaleContext;
            m_pUpscaleContext = NULL;
        }

        // create upscale context
        UpscaleContext::FfxUpscaleInitParams upscaleParams = { pState->m_nUpscaleType, m_bInvertedDepth, m_pDevice, pSwapChain->GetFormat(), &m_UploadHeap, backBufferCount };
        m_pUpscaleContext = UpscaleContext::CreateUpscaleContext(upscaleParams);
    }
    m_pUpscaleContext->OnCreateWindowSizeDependentResources(
        m_renderOutput.GetResource(),
        m_displayOutput.GetResource(),
        pState->renderWidth,
        pState->renderHeight,
        pState->displayWidth,
        pState->displayHeight,
        hdr);
    m_UploadHeap.FlushAndFinish();
}

//--------------------------------------------------------------------------------------
//
// OnDestroyWindowSizeDependentResources
//
//--------------------------------------------------------------------------------------
void Renderer::OnDestroyWindowSizeDependentResources()
{
    m_pGPUParticleSystem->OnReleasingSwapChain();

    m_displayOutput.OnDestroy();
    m_renderOutput.OnDestroy();
    m_OpaqueTexture.OnDestroy();

    m_Bloom.OnDestroyWindowSizeDependentResources();
    m_DownSample.OnDestroyWindowSizeDependentResources();
    
    m_RenderPassJustDepthAndHdr.OnDestroyWindowSizeDependentResources();
    m_RenderPassFullGBuffer.OnDestroyWindowSizeDependentResources();
    m_GBuffer.OnDestroyWindowSizeDependentResources();

    // destroy upscale context
    if (m_pUpscaleContext)
    {
        m_pUpscaleContext->OnDestroyWindowSizeDependentResources();
    }

    m_MagnifierPS.OnDestroyWindowSizeDependentResources();

#if USE_SHADOWMASK
    m_ShadowMask.OnDestroy();
#endif
}

void Renderer::OnUpdateDisplayDependentResources(SwapChain* pSwapChain)
{
    // Update pipelines in case the format of the RTs changed (this happens when going HDR)
    m_ColorConversionPS.UpdatePipelines(pSwapChain->GetFormat(), pSwapChain->GetDisplayMode());
    m_ToneMappingPS.UpdatePipelines(pSwapChain->GetFormat());
    m_ImGUI.UpdatePipeline((pSwapChain->GetDisplayMode() == DISPLAYMODE_SDR) ? pSwapChain->GetFormat() : m_pGBufferHDRTexture->GetFormat());
}

//--------------------------------------------------------------------------------------
//
// LoadScene
//
//--------------------------------------------------------------------------------------
int Renderer::LoadScene(GLTFCommon *pGLTFCommon, int Stage)
{
    // show loading progress
    //
    ImGui::OpenPopup("Loading");
    if (ImGui::BeginPopupModal("Loading", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        float progress = (float)Stage / 13.0f;
        ImGui::ProgressBar(progress, ImVec2(0.f, 0.f), NULL);
        ImGui::EndPopup();
    }

    // use multi threading
    AsyncPool *pAsyncPool = &m_AsyncPool;

    // Loading stages
    //
    if (Stage == 0)
    {
    }
    else if (Stage == 5)
    {
        Profile p("m_pGltfLoader->Load");

        m_pGLTFTexturesAndBuffers = new GLTFTexturesAndBuffers();
        m_pGLTFTexturesAndBuffers->OnCreate(m_pDevice, pGLTFCommon, &m_UploadHeap, &m_VidMemBufferPool, &m_ConstantBufferRing);
    }
    else if (Stage == 6)
    {
        Profile p("LoadTextures");

        // here we are loading onto the GPU all the textures and the inverse matrices
        // this data will be used to create the PBR and Depth passes       
        m_pGLTFTexturesAndBuffers->LoadTextures(pAsyncPool);
    }
    else if (Stage == 7)
    {
        Profile p("m_GLTFDepth->OnCreate");

        //create the glTF's textures, VBs, IBs, shaders and descriptors for this particular pass
        m_GLTFDepth = new GltfDepthPass();
        m_GLTFDepth->OnCreate(
            m_pDevice,
            &m_UploadHeap,
            &m_ResourceViewHeaps,
            &m_ConstantBufferRing,
            &m_VidMemBufferPool,
            m_pGLTFTexturesAndBuffers,
            pAsyncPool,
            DXGI_FORMAT_D32_FLOAT,
            m_bInvertedDepth
        );
    }
    else if (Stage == 9)
    {
        Profile p("m_GLTFPBR->OnCreate");

        // same thing as above but for the PBR pass
        m_GLTFPBR = new GltfPbrPass();
        m_GLTFPBR->OnCreate(
            m_pDevice,
            &m_UploadHeap,
            &m_ResourceViewHeaps,
            &m_ConstantBufferRing,
            m_pGLTFTexturesAndBuffers,
            &m_SkyDome,
            false,                  // use a SSAO mask
            USE_SHADOWMASK,
            &m_RenderPassFullGBuffer,
            pAsyncPool,
            m_bInvertedDepth
        );
    }
    else if (Stage == 10)
    {
        Profile p("m_GLTFBBox->OnCreate");

        // just a bounding box pass that will draw boundingboxes instead of the geometry itself
        m_GLTFBBox = new GltfBBoxPass();
        m_GLTFBBox->OnCreate(
            m_pDevice,
            &m_UploadHeap,
            &m_ResourceViewHeaps,
            &m_ConstantBufferRing,
            &m_VidMemBufferPool,
            m_pGLTFTexturesAndBuffers,
            &m_Wireframe
        );

        // we are borrowing the upload heap command list for uploading to the GPU the IBs and VBs
        m_VidMemBufferPool.UploadData(m_UploadHeap.GetCommandList());

    }
    else if (Stage == 11)
    {
        Profile p("Flush");

        m_UploadHeap.FlushAndFinish();

        //once everything is uploaded we dont need he upload heaps anymore
        m_VidMemBufferPool.FreeUploadHeap();

        // tell caller that we are done loading the map
        return 0;
    }

    Stage++;
    return Stage;
}

//--------------------------------------------------------------------------------------
//
// UnloadScene
//
//--------------------------------------------------------------------------------------
void Renderer::UnloadScene()
{
    // wait for all the async loading operations to finish
    m_AsyncPool.Flush();

    m_pDevice->GPUFlush();

    if (m_GLTFPBR)
    {
        m_GLTFPBR->OnDestroy();
        delete m_GLTFPBR;
        m_GLTFPBR = NULL;
    }

    if (m_GLTFDepth)
    {
        m_GLTFDepth->OnDestroy();
        delete m_GLTFDepth;
        m_GLTFDepth = NULL;
    }

    if (m_GLTFBBox)
    {
        m_GLTFBBox->OnDestroy();
        delete m_GLTFBBox;
        m_GLTFBBox = NULL;
    }

    if (m_pGLTFTexturesAndBuffers)
    {
        m_pGLTFTexturesAndBuffers->OnDestroy();
        delete m_pGLTFTexturesAndBuffers;
        m_pGLTFTexturesAndBuffers = NULL;
    }

    while (!m_shadowMapPool.empty())
    {
        m_shadowMapPool.back().ShadowMap.OnDestroy();
        m_shadowMapPool.pop_back();
    }
}

void Renderer::AllocateShadowMaps(GLTFCommon* pGLTFCommon)
{
    // Go through the lights and allocate shadow information
    uint32_t NumShadows = 0;
    for (int i = 0; i < pGLTFCommon->m_lightInstances.size(); ++i)
    {
        const tfLight& lightData = pGLTFCommon->m_lights[pGLTFCommon->m_lightInstances[i].m_lightId];
        if (lightData.m_shadowResolution)
        {
            SceneShadowInfo ShadowInfo;
            ShadowInfo.ShadowResolution = lightData.m_shadowResolution;
            ShadowInfo.ShadowIndex = NumShadows++;
            ShadowInfo.LightIndex = i;
            m_shadowMapPool.push_back(ShadowInfo);
        }
    }

    if (NumShadows > MaxShadowInstances)
    {
        Trace("Number of shadows has exceeded maximum supported. Please grow value in gltfCommon.h/perFrameStruct.h");
        throw;
    }

    // If we had shadow information, allocate all required maps and bindings
    if (!m_shadowMapPool.empty())
    {
        m_ResourceViewHeaps.AllocDSVDescriptor((uint32_t)m_shadowMapPool.size(), &m_ShadowMapPoolDSV);
        m_ResourceViewHeaps.AllocCBV_SRV_UAVDescriptor((uint32_t)m_shadowMapPool.size(), &m_ShadowMapPoolSRV);

        std::vector<SceneShadowInfo>::iterator CurrentShadow = m_shadowMapPool.begin();
        for( uint32_t i = 0; CurrentShadow < m_shadowMapPool.end(); ++i, ++CurrentShadow)
        {
            CurrentShadow->ShadowMap.InitDepthStencil(m_pDevice, "m_pShadowMap", &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, CurrentShadow->ShadowResolution, CurrentShadow->ShadowResolution, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL), 1.0f);
            CurrentShadow->ShadowMap.CreateDSV(CurrentShadow->ShadowIndex, &m_ShadowMapPoolDSV);
            CurrentShadow->ShadowMap.CreateSRV(CurrentShadow->ShadowIndex, &m_ShadowMapPoolSRV);
        }
    }
}

//--------------------------------------------------------------------------------------
//
// OnRender
//
//--------------------------------------------------------------------------------------
void Renderer::OnRender(UIState* pState, const Camera& Cam, SwapChain* pSwapChain)
{
    const bool bHDR = pSwapChain->GetDisplayMode() != DISPLAYMODE_SDR;
    bool bUseUpscale = pState->m_nUpscaleType != UPSCALE_TYPE_NATIVE;
    bool bUseTaaRcas = pState->bUseTAA || pState->bUseRcas;

    GBuffer* pGBuffer = &m_GBuffer;
    GBufferRenderPass* pRenderPassFullGBuffer = &m_RenderPassFullGBuffer;
    GBufferRenderPass* pRenderPassJustDepthAndHdr =  &m_RenderPassJustDepthAndHdr;
    GltfPbrPass* pGltfPbr = m_GLTFPBR;

    // Timing values
    UINT64 gpuTicksPerSecond;
    m_pDevice->GetGraphicsQueue()->GetTimestampFrequency(&gpuTicksPerSecond);

    // Let our resource managers do some house keeping
    m_CommandListRing.OnBeginFrame();
    m_ConstantBufferRing.OnBeginFrame();
    m_GPUTimer.OnBeginFrame(gpuTicksPerSecond, &m_TimeStamps);

    // Render size viewport
    D3D12_VIEWPORT vpr = { 0.0f, 0.0f, static_cast<float>(pState->renderWidth), static_cast<float>(pState->renderHeight), 0.0f, 1.0f };
    D3D12_RECT srr = { 0, 0, (LONG)pState->renderWidth, (LONG)pState->renderHeight };

    // Display size viewport
    D3D12_VIEWPORT vpd = { 0.0f, 0.0f, static_cast<float>(pState->displayWidth), static_cast<float>(pState->displayHeight), 0.0f, 1.0f };
    D3D12_RECT srd = { 0, 0, (LONG)pState->displayWidth, (LONG)pState->displayHeight };

    m_pUpscaleContext->PreDraw(pState);

    static float fLightModHelper = 2.f;
    float fLightMod = 1.f;

    // Sets the perFrame data 
    per_frame *pPerFrame = NULL;
    if (m_pGLTFTexturesAndBuffers)
    {
        // fill as much as possible using the GLTF (camera, lights, ...)
        pPerFrame = m_pGLTFTexturesAndBuffers->m_pGLTFCommon->SetPerFrameData(Cam);

        // Set some lighting factors
        pPerFrame->iblFactor = pState->IBLFactor;
        pPerFrame->emmisiveFactor = 1.0f;
        pPerFrame->invScreenResolution[0] = 1.0f / ((float)pState->renderWidth);
        pPerFrame->invScreenResolution[1] = 1.0f / ((float)pState->renderHeight);

		pPerFrame->wireframeOptions.setX(pState->WireframeColor[0]);
        pPerFrame->wireframeOptions.setY(pState->WireframeColor[1]);
        pPerFrame->wireframeOptions.setZ(pState->WireframeColor[2]);
        pPerFrame->wireframeOptions.setW(pState->WireframeMode == UIState::WireframeMode::WIREFRAME_MODE_SOLID_COLOR ? 1.0f : 0.0f);
        pPerFrame->lodBias = pState->mipBias;
        m_pGLTFTexturesAndBuffers->SetPerFrameConstants();
        m_pGLTFTexturesAndBuffers->SetSkinningMatricesForSkeletons();
    }

    {
        m_state.flags = IParticleSystem::PF_Streaks | IParticleSystem::PF_DepthCull | IParticleSystem::PF_Sort;
        m_state.flags |= pState->nReactiveMaskMode == REACTIVE_MASK_MODE_ON ? IParticleSystem::PF_Reactive : 0;

        const Camera& camera = pState->camera;
        m_state.constantData.m_ViewProjection = camera.GetProjection() * camera.GetView();
        m_state.constantData.m_View = camera.GetView();
        m_state.constantData.m_ViewInv = math::inverse(camera.GetView());
        m_state.constantData.m_Projection = camera.GetProjection();
        m_state.constantData.m_ProjectionInv = math::inverse(camera.GetProjection());
        m_state.constantData.m_SunDirection = math::Vector4(0.7f, 0.7f, 0, 0);
        m_state.constantData.m_SunColor = math::Vector4(0.8f, 0.8f, 0.7f, 0);
        m_state.constantData.m_AmbientColor = math::Vector4(0.2f, 0.2f, 0.3f, 0);

        m_state.constantData.m_SunColor *= fLightMod;
        m_state.constantData.m_AmbientColor *= fLightMod;

        m_state.constantData.m_FrameTime = pState->m_bPlayAnimations ? (0.001f * (float)pState->deltaTime) : 0.0f;
        PopulateEmitters(pState->m_bPlayAnimations, pState->m_activeScene, 0.001f * (float)pState->deltaTime);
    }

    // command buffer calls
    ID3D12GraphicsCommandList* pCmdLst1 = m_CommandListRing.GetNewCommandList();

    m_GPUTimer.GetTimeStamp(pCmdLst1, "Begin Frame");

    pCmdLst1->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pSwapChain->GetCurrentBackBufferResource(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    // Render shadow maps 
    std::vector<CD3DX12_RESOURCE_BARRIER> ShadowReadBarriers;
    std::vector<CD3DX12_RESOURCE_BARRIER> ShadowWriteBarriers;
    if (m_GLTFDepth && pPerFrame != NULL)
    {
        std::vector<SceneShadowInfo>::iterator ShadowMap = m_shadowMapPool.begin();
        while (ShadowMap < m_shadowMapPool.end())
        {
            pCmdLst1->ClearDepthStencilView(m_ShadowMapPoolDSV.GetCPU(ShadowMap->ShadowIndex), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
            ++ShadowMap;
        }
        m_GPUTimer.GetTimeStamp(pCmdLst1, "Clear shadow maps");

        // Render all shadows
        ShadowMap = m_shadowMapPool.begin();
        while (ShadowMap < m_shadowMapPool.end())
        {
            SetViewportAndScissor(pCmdLst1, 0, 0, ShadowMap->ShadowResolution, ShadowMap->ShadowResolution);
            pCmdLst1->OMSetRenderTargets(0, NULL, false, &m_ShadowMapPoolDSV.GetCPU(ShadowMap->ShadowIndex));
            
            per_frame* cbDepthPerFrame = m_GLTFDepth->SetPerFrameConstants();
            cbDepthPerFrame->mCameraCurrViewProj = pPerFrame->lights[ShadowMap->LightIndex].mLightViewProj;
            cbDepthPerFrame->lodBias = pState->mipBias;

            m_GLTFDepth->Draw(pCmdLst1);

            // Push a barrier
            ShadowReadBarriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(ShadowMap->ShadowMap.GetResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
            ShadowWriteBarriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(ShadowMap->ShadowMap.GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE));

            m_GPUTimer.GetTimeStamp(pCmdLst1, "Shadow map");
            ++ShadowMap;
        }
        
        // Transition all shadow map barriers
        pCmdLst1->ResourceBarrier((UINT)ShadowReadBarriers.size(), ShadowReadBarriers.data());
    }

    // Shadow resolve ---------------------------------------------------------------------------
#if USE_SHADOWMASK
    if (pPerFrame != NULL)
    {
        const D3D12_RESOURCE_BARRIER preShadowResolve[] =
        {
            CD3DX12_RESOURCE_BARRIER::Transition(m_ShadowMask.GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
            CD3DX12_RESOURCE_BARRIER::Transition(m_MotionVectorsDepthMap.GetResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
        };
        pCmdLst1->ResourceBarrier(ARRAYSIZE(preShadowResolve), preShadowResolve);

        ShadowResolveFrame shadowResolveFrame;
        shadowResolveFrame.m_Width = m_Width;
        shadowResolveFrame.m_Height = m_Height;
        shadowResolveFrame.m_ShadowMapSRV = m_ShadowMapSRV;
        shadowResolveFrame.m_DepthBufferSRV = m_MotionVectorsDepthMapSRV;
        shadowResolveFrame.m_ShadowBufferUAV = m_ShadowMaskUAV;

        m_shadowResolve.Draw(pCmdLst1, m_pGLTFTexturesAndBuffers, &shadowResolveFrame);

        const D3D12_RESOURCE_BARRIER postShadowResolve[] =
        {
            CD3DX12_RESOURCE_BARRIER::Transition(m_ShadowMask.GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
            CD3DX12_RESOURCE_BARRIER::Transition(m_MotionVectorsDepthMap.GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE)
        };
        pCmdLst1->ResourceBarrier(ARRAYSIZE(postShadowResolve), postShadowResolve);
    }
    m_GPUTimer.GetTimeStamp(pCmdLst1, "Shadow resolve");
#endif

    // Render Scene to the GBuffer ------------------------------------------------
    if (pPerFrame != NULL)
    {
        pCmdLst1->RSSetViewports(1, &vpr);
        pCmdLst1->RSSetScissorRects(1, &srr);

        if (pGltfPbr)
        {
            const bool bWireframe = pState->WireframeMode != UIState::WireframeMode::WIREFRAME_MODE_OFF;

            std::vector<GltfPbrPass::BatchList> opaque, transparent;
            pGltfPbr->BuildBatchLists(&opaque, &transparent, bWireframe);

            // Render opaque geometry
            {
                pRenderPassFullGBuffer->BeginPass(pCmdLst1, true);
#if USE_SHADOWMASK
                pGltfPbr->DrawBatchList(pCmdLst1, &m_ShadowMaskSRV, &solid, bWireframe);
#else
                pGltfPbr->DrawBatchList(pCmdLst1, &m_ShadowMapPoolSRV, &opaque, bWireframe);
#endif

                if (pState->bRenderAnimatedTextures)
                {
                    m_AnimatedTextures.Render(pCmdLst1, pState->m_bPlayAnimations ? (0.001f * (float)pState->deltaTime) : 0.0f, pState->m_fTextureAnimationSpeed, pState->bCompositionMask, Cam);
                }

                m_GPUTimer.GetTimeStamp(pCmdLst1, "PBR Opaque");
                pRenderPassFullGBuffer->EndPass();
            }

            // draw skydome
            {
                pRenderPassJustDepthAndHdr->BeginPass(pCmdLst1, false);

                // Render skydome
                if (pState->SelectedSkydomeTypeIndex == 1)
                {
                    math::Matrix4 clipToView = math::inverse(pPerFrame->mCameraCurrViewProj);
                    m_SkyDome.Draw(pCmdLst1, clipToView);
                    m_GPUTimer.GetTimeStamp(pCmdLst1, "Skydome cube");
                }
                else if (pState->SelectedSkydomeTypeIndex == 0)
                {
                    SkyDomeProc::Constants skyDomeConstants;
                    skyDomeConstants.invViewProj = math::inverse(pPerFrame->mCameraCurrViewProj);
                    skyDomeConstants.vSunDirection = math::Vector4(1.0f, 0.05f, 0.0f, 0.0f);
                    skyDomeConstants.turbidity = 10.0f;
                    skyDomeConstants.rayleigh = 2.0f;
                    skyDomeConstants.mieCoefficient = 0.005f;
                    skyDomeConstants.mieDirectionalG = 0.8f;
                    skyDomeConstants.luminance = 1.0f;
                    m_SkyDomeProc.Draw(pCmdLst1, skyDomeConstants);

                    m_GPUTimer.GetTimeStamp(pCmdLst1, "Skydome proc");
                }

                pRenderPassJustDepthAndHdr->EndPass();
            }

            {
                // Copy resource before we render transparent stuff
                pCmdLst1->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_OpaqueTexture.GetResource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST));
                pCmdLst1->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_GBuffer.m_HDR.GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE));
                pCmdLst1->CopyResource(m_OpaqueTexture.GetResource(), m_GBuffer.m_HDR.GetResource());
                pCmdLst1->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_OpaqueTexture.GetResource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
                pCmdLst1->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_GBuffer.m_HDR.GetResource(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));

            }

            // draw transparent geometry
            {
                pRenderPassFullGBuffer->BeginPass(pCmdLst1, false);

                if (pState->bRenderParticleSystem)
                {
                    pCmdLst1->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_GBuffer.m_DepthBuffer.GetResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
                    m_pGPUParticleSystem->Render(pCmdLst1, m_ConstantBufferRing, m_state.flags, m_state.emitters, m_state.numEmitters, m_state.constantData);
                    pCmdLst1->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_GBuffer.m_DepthBuffer.GetResource(), D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE));
                }

                std::sort(transparent.begin(), transparent.end());
                pGltfPbr->DrawBatchList(pCmdLst1, &m_ShadowMapPoolSRV, &transparent, bWireframe);
                m_GPUTimer.GetTimeStamp(pCmdLst1, "PBR Transparent");

                pRenderPassFullGBuffer->EndPass();
            }
        }

        // draw object's bounding boxes
        if (m_GLTFBBox && pPerFrame != NULL)
        {
            if (pState->bDrawBoundingBoxes)
            {
                m_GLTFBBox->Draw(pCmdLst1, pPerFrame->mCameraCurrViewProj);

                m_GPUTimer.GetTimeStamp(pCmdLst1, "Bounding Box");
            }
        }

        // draw light's frustums
        if (pState->bDrawLightFrustum && pPerFrame != NULL)
        {
            UserMarker marker(pCmdLst1, "light frustrums");

            math::Vector4 vCenter = math::Vector4(0.0f, 0.0f, 0.5f, 0.0f);
            math::Vector4 vRadius = math::Vector4(1.0f, 1.0f, 0.5f, 0.0f);
            math::Vector4 vColor = math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
            for (uint32_t i = 0; i < pPerFrame->lightCount; i++)
            {
                math::Matrix4 spotlightMatrix = math::inverse(pPerFrame->lights[i].mLightViewProj); 
                math::Matrix4 worldMatrix = pPerFrame->mCameraCurrViewProj * spotlightMatrix;
                m_WireframeBox.Draw(pCmdLst1, &m_Wireframe, worldMatrix, vCenter, vRadius, vColor);
            }

            m_GPUTimer.GetTimeStamp(pCmdLst1, "Light's frustum");
        }
    }

    if (ShadowWriteBarriers.size())
        pCmdLst1->ResourceBarrier((UINT)ShadowWriteBarriers.size(), ShadowWriteBarriers.data());

    D3D12_RESOURCE_BARRIER preResolve[1] = {
        CD3DX12_RESOURCE_BARRIER::Transition(pGBuffer->m_HDR.GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
    };
    pCmdLst1->ResourceBarrier(1, preResolve);

    // if FSR2 and auto reactive mask is enabled: generate reactive mask
    if (pState->nReactiveMaskMode == REACTIVE_MASK_MODE_AUTOGEN)
    {
        UpscaleContext::FfxUpscaleSetup upscaleSetup;
        upscaleSetup.cameraSetup.vCameraPos = pState->camera.GetPosition();
        upscaleSetup.cameraSetup.mCameraView = pState->camera.GetView();
        upscaleSetup.cameraSetup.mCameraViewInv = math::inverse(pState->camera.GetView());
        upscaleSetup.cameraSetup.mCameraProj = pState->camera.GetProjection();
        upscaleSetup.opaqueOnlyColorResource = m_OpaqueTexture.GetResource();
        upscaleSetup.unresolvedColorResource = m_GBuffer.m_HDR.GetResource();
        upscaleSetup.motionvectorResource = m_GBuffer.m_MotionVectors.GetResource();
        upscaleSetup.depthbufferResource = m_GBuffer.m_DepthBuffer.GetResource();
        upscaleSetup.reactiveMapResource = m_GBuffer.m_UpscaleReactive.GetResource();
        upscaleSetup.transparencyAndCompositionResource = m_GBuffer.m_UpscaleTransparencyAndComposition.GetResource();
        upscaleSetup.resolvedColorResource = m_displayOutput.GetResource();
        m_pUpscaleContext->GenerateReactiveMask(pCmdLst1, upscaleSetup, pState);
    }

    // Post proc---------------------------------------------------------------------------

    // Bloom, takes HDR as input and applies bloom to it.
    {
        D3D12_CPU_DESCRIPTOR_HANDLE renderTargets[] = { pGBuffer->m_HDRRTV.GetCPU() };
        pCmdLst1->OMSetRenderTargets(ARRAYSIZE(renderTargets), renderTargets, false, NULL);

        m_DownSample.Draw(pCmdLst1);
        m_GPUTimer.GetTimeStamp(pCmdLst1, "Downsample");

        m_Bloom.Draw(pCmdLst1, &pGBuffer->m_HDR);
        m_GPUTimer.GetTimeStamp(pCmdLst1, "Bloom");
    }

    // Start tracking input/output resources at this point to handle HDR and SDR render paths 
    ID3D12Resource*                 pRscCurrentInput = pGBuffer->m_HDR.GetResource();
    CBV_SRV_UAV                     SRVCurrentInput  = pGBuffer->m_HDRSRV;
    D3D12_CPU_DESCRIPTOR_HANDLE     RTVCurrentOutput = pGBuffer->m_HDRRTV.GetCPU();
    CBV_SRV_UAV                     UAVCurrentOutput = pGBuffer->m_HDRUAV;

    // Always use upscale context, if we don't want one, Spatial can skip upscale (but still do TAA and/or RCAS)
    if(bUseUpscale || bUseTaaRcas)
    {
        pCmdLst1->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pGBuffer->m_HDR.GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE| D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
        pCmdLst1->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pGBuffer->m_MotionVectors.GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
        pCmdLst1->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pGBuffer->m_UpscaleReactive.GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
        pCmdLst1->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pGBuffer->m_UpscaleTransparencyAndComposition.GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
        pCmdLst1->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pGBuffer->m_DepthBuffer.GetResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

        UpscaleContext::FfxUpscaleSetup upscaleSetup;
        upscaleSetup.cameraSetup.vCameraPos = pState->camera.GetPosition();
        upscaleSetup.cameraSetup.mCameraView = pState->camera.GetView();
        upscaleSetup.cameraSetup.mCameraViewInv = math::inverse(pState->camera.GetView());
        upscaleSetup.cameraSetup.mCameraProj = pState->camera.GetProjection();
        upscaleSetup.opaqueOnlyColorResource = m_OpaqueTexture.GetResource();
        upscaleSetup.unresolvedColorResource = m_GBuffer.m_HDR.GetResource();
        upscaleSetup.motionvectorResource = m_GBuffer.m_MotionVectors.GetResource();
        upscaleSetup.depthbufferResource = m_GBuffer.m_DepthBuffer.GetResource();
        upscaleSetup.reactiveMapResource = m_GBuffer.m_UpscaleReactive.GetResource();
        upscaleSetup.transparencyAndCompositionResource = m_GBuffer.m_UpscaleTransparencyAndComposition.GetResource();
        upscaleSetup.resolvedColorResource = m_displayOutput.GetResource();

        pCmdLst1->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_displayOutput.GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

        m_pUpscaleContext->Draw(pCmdLst1, upscaleSetup, pState);

        pCmdLst1->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_displayOutput.GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RENDER_TARGET));

        SRVCurrentInput = m_displayOutputSRV;
        {
            vpr = { 0.0f, 0.0f, static_cast<float>(pState->displayWidth), static_cast<float>(pState->displayHeight), 0.0f, 1.0f };
            srr = { 0, 0, (LONG)pState->displayWidth, (LONG)pState->displayHeight };
        }

        pCmdLst1->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pGBuffer->m_DepthBuffer.GetResource(), D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE));
        pCmdLst1->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pGBuffer->m_MotionVectors.GetResource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));
        pCmdLst1->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pGBuffer->m_UpscaleReactive.GetResource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));
        pCmdLst1->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pGBuffer->m_UpscaleTransparencyAndComposition.GetResource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));
        pCmdLst1->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pGBuffer->m_HDR.GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

        if(bUseUpscale)
            m_GPUTimer.GetTimeStamp(pCmdLst1, m_pUpscaleContext->Name().c_str());
        else if(pState->bUseTAA && !pState->bUseRcas)
            m_GPUTimer.GetTimeStamp(pCmdLst1, "TAA");
        else if (!pState->bUseTAA && pState->bUseRcas)
            m_GPUTimer.GetTimeStamp(pCmdLst1, "RCAS");
        else if (pState->bUseTAA && pState->bUseRcas)
            m_GPUTimer.GetTimeStamp(pCmdLst1, "TAA & RCAS");

        pCmdLst1->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_displayOutput.GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
        pCmdLst1->OMSetRenderTargets(1, &m_displayOutputRTV.GetCPU(), true, NULL);
        pRscCurrentInput = m_displayOutput.GetResource();
        SRVCurrentInput = m_displayOutputSRV;
        RTVCurrentOutput = m_displayOutputRTV.GetCPU();
        UAVCurrentOutput = m_displayOutputUAV;
    }

    // Upscale done, display size is now the main target
    pCmdLst1->RSSetViewports(1, &vpd);
    pCmdLst1->RSSetScissorRects(1, &srd);

    // If using FreeSync HDR we need to do the tonemapping in-place and then apply the GUI, later we'll apply the color conversion into the swapchain
    if (bHDR)
    {
        // Magnifier Pass: m_HDR as input, pass' own output
        if (pState->bUseMagnifier)
        {
            // Note: assumes m_GBuffer.HDR is in D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
            m_MagnifierPS.Draw(pCmdLst1, pState->MagnifierParams, SRVCurrentInput);
            m_GPUTimer.GetTimeStamp(pCmdLst1, "Magnifier");

            pRscCurrentInput = m_MagnifierPS.GetPassOutputResource();
            SRVCurrentInput = m_MagnifierPS.GetPassOutputSRV();
            RTVCurrentOutput = m_MagnifierPS.GetPassOutputRTV().GetCPU();
            UAVCurrentOutput = m_MagnifierPS.GetPassOutputUAV();

            pCmdLst1->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_MagnifierPS.GetPassOutputResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
        }

        // In place Tonemapping ------------------------------------------------------------------------
        {
            D3D12_RESOURCE_BARRIER inputRscToUAV = CD3DX12_RESOURCE_BARRIER::Transition(pRscCurrentInput, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            pCmdLst1->ResourceBarrier(1, &inputRscToUAV);

            m_ToneMappingCS.Draw(pCmdLst1, &UAVCurrentOutput, pState->ExposureHdr, pState->SelectedTonemapperIndex, pState->displayWidth, pState->displayHeight);

            D3D12_RESOURCE_BARRIER inputRscToSRV = CD3DX12_RESOURCE_BARRIER::Transition(pRscCurrentInput, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RENDER_TARGET);
            pCmdLst1->ResourceBarrier(1, &inputRscToSRV);
        }

        // Render HUD  ------------------------------------------------------------------------
        {
            pCmdLst1->OMSetRenderTargets(1, &RTVCurrentOutput, true, NULL);
            pCmdLst1->RSSetViewports(1, &vpd);
            pCmdLst1->RSSetScissorRects(1, &srd);

            m_ImGUI.Draw(pCmdLst1);

            m_GPUTimer.GetTimeStamp(pCmdLst1, "ImGUI Rendering");

            // reset viewport/scissorRects after rendering UI
            pCmdLst1->RSSetViewports(1, &vpd);
            pCmdLst1->RSSetScissorRects(1, &srd);

            D3D12_RESOURCE_BARRIER inputRscToSRV = CD3DX12_RESOURCE_BARRIER::Transition(pRscCurrentInput, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            pCmdLst1->ResourceBarrier(1, &inputRscToSRV);
        }
    }
    
    // Keep tracking input/output resource views 
    RTVCurrentOutput = *pSwapChain->GetCurrentBackBufferRTV();
    UAVCurrentOutput = {}; // no BackBufferUAV.

    ID3D12GraphicsCommandList* pCmdLst2 = pCmdLst1;

    if (bHDR)
    {
        pCmdLst2->OMSetRenderTargets(1, pSwapChain->GetCurrentBackBufferRTV(), true, NULL);

        // FS HDR mode! Apply color conversion now.
        m_ColorConversionPS.Draw(pCmdLst2, &SRVCurrentInput);
        m_GPUTimer.GetTimeStamp(pCmdLst2, "Color conversion");
    }
    else
    {
        // Magnifier Pass: m_HDR as input, pass' own output
        if (pState->bUseMagnifier)
        {
            // Note: assumes m_GBuffer.HDR is in D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
            m_MagnifierPS.Draw(pCmdLst2, pState->MagnifierParams, SRVCurrentInput);
            m_GPUTimer.GetTimeStamp(pCmdLst2, "Magnifier");

            pRscCurrentInput = m_MagnifierPS.GetPassOutputResource();
            SRVCurrentInput = m_MagnifierPS.GetPassOutputSRV();
            RTVCurrentOutput = m_MagnifierPS.GetPassOutputRTV().GetCPU();
            UAVCurrentOutput = m_MagnifierPS.GetPassOutputUAV();
            pCmdLst2->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_MagnifierPS.GetPassOutputResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
        }

        // Tonemapping ------------------------------------------------------------------------
        {
            pCmdLst2->OMSetRenderTargets(1, pSwapChain->GetCurrentBackBufferRTV(), true, NULL);
            pCmdLst2->RSSetScissorRects(1, &srd);

            m_ToneMappingPS.Draw(pCmdLst2, &SRVCurrentInput, pState->Exposure, pState->SelectedTonemapperIndex);
            m_GPUTimer.GetTimeStamp(pCmdLst2, "Tone mapping");
        }

        // Render HUD to swapchain directly ------------------------------------------------------------------------
        {
            m_ImGUI.Draw(pCmdLst2);
            m_GPUTimer.GetTimeStamp(pCmdLst2, "ImGUI Rendering");
        }
    }

    pCmdLst2->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pGBuffer->m_HDR.GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));
    if (bUseUpscale || bUseTaaRcas)
        pCmdLst2->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_displayOutput.GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));
    if (pState->bUseMagnifier)
        pCmdLst2->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_MagnifierPS.GetPassOutputResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));


    if (!m_pScreenShotName.empty())
    {
        m_SaveTexture.CopyRenderTargetIntoStagingTexture(m_pDevice->GetDevice(), pCmdLst2, pSwapChain->GetCurrentBackBufferResource(), D3D12_RESOURCE_STATE_RENDER_TARGET);
    }

    // Transition swapchain into present mode

    pCmdLst2->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pSwapChain->GetCurrentBackBufferResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    if (pState->bEnableFrameRateLimiter &&
        pState->bUseGPUFrameRateLimiter &&
        !m_TimeStamps.empty())
    {
        m_GpuFrameRateLimiter.Draw(pCmdLst2, &m_ConstantBufferRing, uint64_t(m_TimeStamps.back().m_microseconds), uint64_t(pState->targetFrameTime * 1000));
        m_GPUTimer.GetTimeStamp(pCmdLst2, "FrameRateLimiter");
    }

    m_GPUTimer.OnEndFrame();

    m_GPUTimer.CollectTimings(pCmdLst2);

    // Close & Submit the command list #2 -------------------------------------------------
    ThrowIfFailed(pCmdLst2->Close());

    ID3D12CommandList* CmdListList2[] = { pCmdLst2 };
    m_pDevice->GetGraphicsQueue()->ExecuteCommandLists(1, CmdListList2);

    // Handle screenshot request
    if (!m_pScreenShotName.empty())
    {
        m_SaveTexture.SaveStagingTextureAsJpeg(m_pDevice->GetDevice(), m_pDevice->GetGraphicsQueue(), m_pScreenShotName.c_str());
        m_pScreenShotName.clear();
    }

    // wait for swapchain: ensure timings in the sample are correct, not something you'd want to do in a title
    pSwapChain->WaitForSwapChain();
}

void Renderer::ResetScene()
{
    ZeroMemory(m_EmissionRates, sizeof(m_EmissionRates));

    // Reset the particle system when the scene changes so no particles from the previous scene persist
    m_pGPUParticleSystem->Reset();
}

void Renderer::PopulateEmitters(bool playAnimations, int activeScene, float frameTime)
{
    IParticleSystem::EmitterParams sparksEmitter = {};
    IParticleSystem::EmitterParams smokeEmitter = {};

    sparksEmitter.m_NumToEmit = 0;
    sparksEmitter.m_ParticleLifeSpan = 1.0f;
    sparksEmitter.m_StartSize  = 0.6f * 0.02f;
    sparksEmitter.m_EndSize    = 0.4f * 0.02f;
    sparksEmitter.m_VelocityVariance = 1.5f;
    sparksEmitter.m_Mass = 1.0f;
    sparksEmitter.m_TextureIndex = 1;
    sparksEmitter.m_Streaks = true;

    smokeEmitter.m_NumToEmit = 0;
    smokeEmitter.m_ParticleLifeSpan = 50.0f;
    smokeEmitter.m_StartSize  = 0.4f;
    smokeEmitter.m_EndSize    = 1.0f;
    smokeEmitter.m_VelocityVariance = 1.0f;
    smokeEmitter.m_Mass = 0.0003f;
    smokeEmitter.m_TextureIndex = 0;
    smokeEmitter.m_Streaks = false;

    if ( activeScene == 0 ) // scene 0 = warehouse
    {
        m_state.numEmitters = 2;
        m_state.emitters[0] = sparksEmitter;
        m_state.emitters[1] = sparksEmitter;

        m_state.emitters[0].m_Position = math::Vector4(-4.15f, -1.85f, -3.8f, 1.0f);
        m_state.emitters[0].m_PositionVariance = math::Vector4(0.1f, 0.0f, 0.0f, 1.0f);
        m_state.emitters[0].m_Velocity = math::Vector4(0.0f,  0.08f, 0.8f, 1.0f);
        m_EmissionRates[0].m_ParticlesPerSecond = 300.0f;

        m_state.emitters[1].m_Position = math::Vector4(-4.9f, -1.5f, -4.8f, 1.0f);
        m_state.emitters[1].m_PositionVariance = math::Vector4(0.0f, 0.0f, 0.0f, 1.0f);
        m_state.emitters[1].m_Velocity = math::Vector4(0.0f, 0.8f, -0.8f, 1.0f);
        m_EmissionRates[1].m_ParticlesPerSecond = 400.0f;

        m_state.constantData.m_StartColor[0] = math::Vector4(10.0f, 10.0f, 2.0f, 0.9f);
        m_state.constantData.m_EndColor[0] = math::Vector4(10.0f, 10.0f, 0.0f, 0.1f);
        m_state.constantData.m_StartColor[1] = math::Vector4(10.0f, 10.0f, 2.0f, 0.9f);
        m_state.constantData.m_EndColor[1] = math::Vector4(10.0f, 10.0f, 0.0f, 0.1f);
    }
    else if ( activeScene == 1 ) // Sponza
    {
        m_state.numEmitters = 2;
        m_state.emitters[0] = smokeEmitter;
        m_state.emitters[1] = sparksEmitter;

        m_state.emitters[0].m_Position = math::Vector4(-13.0f, 0.0f, 1.4f, 1.0f);
        m_state.emitters[0].m_PositionVariance = math::Vector4(0.1f, 0.0f, 0.1f, 1.0f);
        m_state.emitters[0].m_Velocity = math::Vector4(0.0f, 0.2f, 0.0f, 1.0f);
        m_EmissionRates[0].m_ParticlesPerSecond = 10.0f;

        m_state.emitters[1].m_Position = math::Vector4(-13.0f, 0.0f, -1.4f, 1.0f);
        m_state.emitters[1].m_PositionVariance = math::Vector4(0.05f, 0.0f, 0.05f, 1.0f);
        m_state.emitters[1].m_Velocity = math::Vector4(0.0f, 4.0f, 0.0f, 1.0f);
        m_state.emitters[1].m_VelocityVariance = 0.5f;
        m_state.emitters[1].m_StartSize = 0.02f;
        m_state.emitters[1].m_EndSize = 0.02f;
        m_state.emitters[1].m_Mass = 1.0f;
        m_EmissionRates[1].m_ParticlesPerSecond = 500.0f;

        m_state.constantData.m_StartColor[0] = math::Vector4(0.3f, 0.3f, 0.3f, 0.4f);
        m_state.constantData.m_EndColor[0] = math::Vector4(0.4f, 0.4f, 0.4f, 0.1f);
        m_state.constantData.m_StartColor[1] = math::Vector4(10.0f, 10.0f, 10.0f, 0.9f);
        m_state.constantData.m_EndColor[1] = math::Vector4(5.0f, 8.0f, 5.0f, 0.1f);
    }

    // Update all our active emitters so we know how many whole numbers of particles to emit from each emitter this frame
    for (int i = 0; i < m_state.numEmitters; i++)
    {
        m_state.constantData.m_EmitterLightingCenter[i] = m_state.emitters[ i ].m_Position;

        if (m_EmissionRates[i].m_ParticlesPerSecond > 0.0f)
        {
            m_EmissionRates[i].m_Accumulation += m_EmissionRates[i].m_ParticlesPerSecond * (playAnimations ? frameTime : 0.0f);

            if (m_EmissionRates[i].m_Accumulation > 1.0f)
            {
                float integerPart = 0.0f;
                float fraction = modf(m_EmissionRates[i].m_Accumulation, &integerPart);

                m_state.emitters[i].m_NumToEmit = (int)integerPart;
                m_EmissionRates[i].m_Accumulation = fraction;
            }
        }
    }
}


void Renderer::BuildDevUI(UIState* pState)
{
    if (m_pUpscaleContext)
    {
        m_pUpscaleContext->BuildDevUI(pState);
    }
}
