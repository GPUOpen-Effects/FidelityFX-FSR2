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
void Renderer::OnCreate(Device *pDevice, SwapChain *pSwapChain, float FontSize, bool bInvertedDepth)
{
    m_pDevice = pDevice;
    m_bInvertedDepth = bInvertedDepth;

    // Create all the heaps for the resources views
    const uint32_t cbvDescriptorCount = 2000;
    const uint32_t srvDescriptorCount = 8000;
    const uint32_t uavDescriptorCount = 10;
    const uint32_t samplerDescriptorCount = 20;
    m_ResourceViewHeaps.OnCreate(pDevice, cbvDescriptorCount, srvDescriptorCount, uavDescriptorCount, samplerDescriptorCount);

    // Create a commandlist ring for the Direct queue
    uint32_t commandListsPerBackBuffer = 8;
    m_CommandListRing.OnCreate(pDevice, backBufferCount, commandListsPerBackBuffer);

    // Create a 'dynamic' constant buffer
    const uint32_t constantBuffersMemSize = 32 * 1024 * 1024;
    m_ConstantBufferRing.OnCreate(pDevice, backBufferCount, constantBuffersMemSize, "Uniforms");

    // Create a 'static' pool for vertices and indices 
    const uint32_t staticGeometryMemSize = (5 * 128) * 1024 * 1024;
    m_VidMemBufferPool.OnCreate(pDevice, staticGeometryMemSize, true, "StaticGeom");

    // Create a 'static' pool for vertices and indices in system memory
    //const uint32_t systemGeometryMemSize = 16 * 1024;
   // m_SysMemBufferPool.OnCreate(pDevice, systemGeometryMemSize, false, "PostProcGeom");

    // initialize the GPU time stamps module
    m_GPUTimer.OnCreate(pDevice, backBufferCount);

    // Quick helper to upload resources, it has it's own commandList and uses suballocation.
    const uint32_t uploadHeapMemSize = 1000 * 1024 * 1024;
    m_UploadHeap.OnCreate(pDevice, uploadHeapMemSize);    // initialize an upload heap (uses suballocation for faster results)

    // Create GBuffer and render passes
    //
    {
        m_GBuffer.OnCreate(
            pDevice, 
            &m_ResourceViewHeaps, 
            {
                { GBUFFER_DEPTH, VK_FORMAT_D32_SFLOAT},
                { GBUFFER_FORWARD, VK_FORMAT_R16G16B16A16_SFLOAT},
                { GBUFFER_MOTION_VECTORS, VK_FORMAT_R16G16_SFLOAT},
                { GBUFFER_UPSCALEREACTIVE, VK_FORMAT_R8_UNORM},
                { GBUFFER_UPSCALE_TRANSPARENCY_AND_COMPOSITION, VK_FORMAT_R8_UNORM},
            },
            1
        );

        GBufferFlags fullGBuffer = GBUFFER_DEPTH | GBUFFER_FORWARD | GBUFFER_MOTION_VECTORS | GBUFFER_UPSCALEREACTIVE | GBUFFER_UPSCALE_TRANSPARENCY_AND_COMPOSITION;
        if (bInvertedDepth) fullGBuffer |= GBUFFER_INVERTED_DEPTH;

        bool bClear = true;
        m_RenderPassFullGBufferWithClear.OnCreate(&m_GBuffer, fullGBuffer, bClear, "m_RenderPassFullGBufferWithClear");
        m_RenderPassFullGBuffer.OnCreate(&m_GBuffer, fullGBuffer, !bClear, "m_RenderPassFullGBuffer");
        m_RenderPassJustDepthAndHdr.OnCreate(&m_GBuffer, GBUFFER_DEPTH | GBUFFER_FORWARD, !bClear, "m_RenderPassJustDepthAndHdr");
        m_RenderPassFullGBufferNoDepthWrite.OnCreate(&m_GBuffer, fullGBuffer, !bClear, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, "m_RenderPassFullGBufferNoDepthWrite");
    }

    // Create render pass shadow, will clear contents
    {
        VkAttachmentDescription depthAttachments;
        AttachClearBeforeUse(VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, &depthAttachments);
        m_Render_pass_shadow = CreateRenderPassOptimal(m_pDevice->GetDevice(), 0, NULL, &depthAttachments, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    }

    m_SkyDome.OnCreate(pDevice, m_RenderPassJustDepthAndHdr.GetRenderPass(), &m_UploadHeap, VK_FORMAT_R16G16B16A16_SFLOAT, &m_ResourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, "..\\media\\cauldron-media\\envmaps\\papermill\\diffuse.dds", "..\\media\\cauldron-media\\envmaps\\papermill\\specular.dds", VK_SAMPLE_COUNT_1_BIT, m_bInvertedDepth);
    m_SkyDomeProc.OnCreate(pDevice, m_RenderPassJustDepthAndHdr.GetRenderPass(), &m_UploadHeap, VK_FORMAT_R16G16B16A16_SFLOAT, &m_ResourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, VK_SAMPLE_COUNT_1_BIT, m_bInvertedDepth);
    m_Wireframe.OnCreate(pDevice, m_RenderPassJustDepthAndHdr.GetRenderPass(), &m_ResourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, VK_SAMPLE_COUNT_1_BIT, m_bInvertedDepth);
    m_WireframeBox.OnCreate(pDevice, &m_ResourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool);
    m_DownSample.OnCreate(pDevice, &m_ResourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, VK_FORMAT_R16G16B16A16_SFLOAT);
    m_Bloom.OnCreate(pDevice, &m_ResourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, VK_FORMAT_R16G16B16A16_SFLOAT);
    m_MagnifierPS.OnCreate(pDevice, &m_ResourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, VK_FORMAT_R16G16B16A16_SFLOAT);

    // Create tonemapping pass
    m_ToneMappingCS.OnCreate(pDevice, &m_ResourceViewHeaps, &m_ConstantBufferRing);
    m_ToneMappingPS.OnCreate(m_pDevice, pSwapChain->GetRenderPass(), &m_ResourceViewHeaps, &m_VidMemBufferPool, &m_ConstantBufferRing);
    m_ColorConversionPS.OnCreate(pDevice, pSwapChain->GetRenderPass(), &m_ResourceViewHeaps, &m_VidMemBufferPool, &m_ConstantBufferRing);

    // Initialize UI rendering resources
    m_ImGUI.OnCreate(m_pDevice, pSwapChain->GetRenderPass(), &m_UploadHeap, &m_ConstantBufferRing, FontSize);

    // Make sure upload heap has finished uploading before continuing
    m_VidMemBufferPool.UploadData(m_UploadHeap.GetCommandList());
    m_UploadHeap.FlushAndFinish();

    m_pGPUParticleSystem = IParticleSystem::CreateGPUSystem("..\\media\\atlas.dds");
    m_pGPUParticleSystem->OnCreateDevice(*pDevice, m_UploadHeap, m_ResourceViewHeaps, m_VidMemBufferPool, m_ConstantBufferRing, m_RenderPassFullGBufferNoDepthWrite.GetRenderPass());

    m_GpuFrameRateLimiter.OnCreate(pDevice, &m_ConstantBufferRing, &m_ResourceViewHeaps);

    m_AnimatedTextures.OnCreate( *pDevice, m_UploadHeap, m_VidMemBufferPool, m_RenderPassFullGBufferWithClear.GetRenderPass(), m_ResourceViewHeaps, m_ConstantBufferRing );

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
    m_ToneMappingPS.OnDestroy();
    m_ToneMappingCS.OnDestroy();
    m_Bloom.OnDestroy();
    m_DownSample.OnDestroy();
    m_MagnifierPS.OnDestroy();
    m_WireframeBox.OnDestroy();
    m_Wireframe.OnDestroy();
    m_SkyDomeProc.OnDestroy();
    m_SkyDome.OnDestroy();

    m_RenderPassFullGBufferNoDepthWrite.OnDestroy();
    m_RenderPassFullGBufferWithClear.OnDestroy();
    m_RenderPassJustDepthAndHdr.OnDestroy();
    m_RenderPassFullGBuffer.OnDestroy();
    m_GBuffer.OnDestroy();

    if (m_pUpscaleContext)
    {
        m_pUpscaleContext->OnDestroy();
        delete m_pUpscaleContext;
        m_pUpscaleContext = NULL;
    }

    vkDestroyRenderPass(m_pDevice->GetDevice(), m_Render_pass_shadow, nullptr);
       
    m_UploadHeap.OnDestroy();
    m_GPUTimer.OnDestroy();
    m_VidMemBufferPool.OnDestroy();
    m_SysMemBufferPool.OnDestroy();
    m_ConstantBufferRing.OnDestroy();
    m_ResourceViewHeaps.OnDestroy();
    m_CommandListRing.OnDestroy();    
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
   
    // Create GBuffer
    //
    m_GBuffer.OnCreateWindowSizeDependentResources(pSwapChain, m_Width, m_Height);

    // Create frame buffers for the GBuffer render passes
    //
    m_RenderPassFullGBufferWithClear.OnCreateWindowSizeDependentResources(m_Width, m_Height);
    m_RenderPassJustDepthAndHdr.OnCreateWindowSizeDependentResources(m_Width, m_Height);
    m_RenderPassFullGBuffer.OnCreateWindowSizeDependentResources(m_Width, m_Height);
    m_RenderPassFullGBufferNoDepthWrite.OnCreateWindowSizeDependentResources(m_Width, m_Height);

    bool renderNative = (pState->m_nUpscaleType == UPSCALE_TYPE_NATIVE);
    bool hdr = (pSwapChain->GetDisplayMode() != DISPLAYMODE_SDR);
    VkFormat uiFormat = (hdr ? m_GBuffer.m_HDR.GetFormat() : pSwapChain->GetFormat());
    VkFormat dFormat = (hdr ? uiFormat : VK_FORMAT_R8G8B8A8_UNORM);

    m_displayOutput.InitRenderTarget(m_pDevice, pState->displayWidth, pState->displayHeight, dFormat, VK_SAMPLE_COUNT_1_BIT, (VkImageUsageFlags)(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT), false, "DisplayOutput");
    m_displayOutput.CreateSRV(&m_displayOutputSRV);

    {
        m_OpaqueTexture.InitRenderTarget(m_pDevice, pState->renderWidth, pState->renderHeight, m_GBuffer.m_HDR.GetFormat(), VK_SAMPLE_COUNT_1_BIT, (VkImageUsageFlags)(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT), false, "OpaqueBuffer");
        m_OpaqueTexture.CreateSRV(&m_OpaqueTextureSRV);
    }

    // Update PostProcessing passes
    //
    m_DownSample.OnCreateWindowSizeDependentResources(m_Width, m_Height, &m_GBuffer.m_HDR, 6); //downsample the HDR texture 6 times
    m_Bloom.OnCreateWindowSizeDependentResources(m_Width / 2, m_Height / 2, m_DownSample.GetTexture(), 6, &m_GBuffer.m_HDR);
    m_MagnifierPS.OnCreateWindowSizeDependentResources(pState->displayWidth, pState->displayHeight);
    m_bMagResourceReInit = true;

    m_RenderPassDisplayOutput = SimpleColorBlendRenderPass(m_pDevice->GetDevice(), m_displayOutput.GetFormat(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    std::vector<VkImageView> attachments = { m_displayOutputSRV };
    m_FramebufferDisplayOutput = CreateFrameBuffer(m_pDevice->GetDevice(), m_RenderPassDisplayOutput, &attachments, pState->displayWidth, pState->displayHeight);

    m_ImGUI.UpdatePipeline((pSwapChain->GetDisplayMode() == DISPLAYMODE_SDR) ? pSwapChain->GetRenderPass() : m_RenderPassDisplayOutput);

    m_pGPUParticleSystem->OnResizedSwapChain(pState->renderWidth, pState->renderHeight, m_GBuffer.m_DepthBuffer, m_RenderPassFullGBufferNoDepthWrite.GetFramebuffer());

    // Lazy Upscale context generation: 
    if ((m_pUpscaleContext == NULL) || (pState->m_nUpscaleType != m_pUpscaleContext->Type()))
    {
        // just a safeguard, usually OnDestroyWindowSizeDependentResources should have been called before it enters here
        if (m_pUpscaleContext)
        {
            m_pUpscaleContext->OnDestroyWindowSizeDependentResources();
            m_pUpscaleContext->OnDestroy();
            delete m_pUpscaleContext;
            m_pUpscaleContext = NULL;
        }

        // create upscale context
        UpscaleContext::FfxUpscaleInitParams upscaleParams = { pState->m_nUpscaleType, m_bInvertedDepth, m_pDevice, pSwapChain->GetFormat(), &m_UploadHeap, backBufferCount };
        m_pUpscaleContext = UpscaleContext::CreateUpscaleContext(upscaleParams);
    }
    m_pUpscaleContext->OnCreateWindowSizeDependentResources(nullptr, m_displayOutputSRV, pState->renderWidth, pState->renderHeight, pState->displayWidth, pState->displayHeight, true);
}

//--------------------------------------------------------------------------------------
//
// OnDestroyWindowSizeDependentResources
//
//--------------------------------------------------------------------------------------
void Renderer::OnDestroyWindowSizeDependentResources()
{
    m_pDevice->GPUFlush();

    m_pGPUParticleSystem->OnReleasingSwapChain();

    vkDestroyImageView(m_pDevice->GetDevice(), m_OpaqueTextureSRV, 0);
    vkDestroyFramebuffer(m_pDevice->GetDevice(), m_FramebufferDisplayOutput, 0);
    vkDestroyImageView(m_pDevice->GetDevice(), m_displayOutputSRV, 0);
    vkDestroyRenderPass(m_pDevice->GetDevice(), m_RenderPassDisplayOutput, 0);
    
    m_OpaqueTextureSRV = nullptr;
    m_FramebufferDisplayOutput = nullptr;
    m_displayOutputSRV = nullptr;
    m_RenderPassDisplayOutput = nullptr;

    m_displayOutput.OnDestroy();
    m_Bloom.OnDestroyWindowSizeDependentResources();
    m_DownSample.OnDestroyWindowSizeDependentResources();
    m_MagnifierPS.OnDestroyWindowSizeDependentResources();

    m_OpaqueTexture.OnDestroy();

    m_RenderPassFullGBufferWithClear.OnDestroyWindowSizeDependentResources();
    m_RenderPassJustDepthAndHdr.OnDestroyWindowSizeDependentResources();
    m_RenderPassFullGBuffer.OnDestroyWindowSizeDependentResources();
    m_RenderPassFullGBufferNoDepthWrite.OnDestroyWindowSizeDependentResources();
    m_GBuffer.OnDestroyWindowSizeDependentResources();

    // destroy upscale context
    if (m_pUpscaleContext)
    {
        m_pUpscaleContext->OnDestroyWindowSizeDependentResources();
    }
}

void Renderer::OnUpdateDisplayDependentResources(SwapChain *pSwapChain)
{
    // Update the pipelines if the swapchain render pass has changed (for example when the format of the swapchain changes)
    //
    m_ColorConversionPS.UpdatePipelines(pSwapChain->GetRenderPass(), pSwapChain->GetDisplayMode());
    m_ToneMappingPS.UpdatePipelines(pSwapChain->GetRenderPass());
}

//--------------------------------------------------------------------------------------
//
// OnUpdateLocalDimmingChangedResources
//
//--------------------------------------------------------------------------------------
void Renderer::OnUpdateLocalDimmingChangedResources(SwapChain *pSwapChain)
{
    m_ColorConversionPS.UpdatePipelines(pSwapChain->GetRenderPass(), pSwapChain->GetDisplayMode());
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
        float progress = (float)Stage / 12.0f;
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
            m_Render_pass_shadow,
            &m_UploadHeap,
            &m_ResourceViewHeaps,
            &m_ConstantBufferRing,
            &m_VidMemBufferPool,
            m_pGLTFTexturesAndBuffers,
            pAsyncPool,
            m_bInvertedDepth
        );

        m_VidMemBufferPool.UploadData(m_UploadHeap.GetCommandList());
        m_UploadHeap.FlushAndFinish();
    }
    else if (Stage == 8)
    {
        Profile p("m_GLTFPBR->OnCreate");

        // same thing as above but for the PBR pass
        m_GLTFPBR = new GltfPbrPass();
        m_GLTFPBR->OnCreate(
            m_pDevice,
            &m_UploadHeap,
            &m_ResourceViewHeaps,
            &m_ConstantBufferRing,
            &m_VidMemBufferPool,
            m_pGLTFTexturesAndBuffers,
            &m_SkyDome,
            false, // use SSAO mask
            m_ShadowSRVPool,
            &m_RenderPassFullGBufferWithClear,
            pAsyncPool,
            m_bInvertedDepth
        );

        m_VidMemBufferPool.UploadData(m_UploadHeap.GetCommandList());
        m_UploadHeap.FlushAndFinish();
    }
    else if (Stage == 9)
    {
        Profile p("m_GLTFBBox->OnCreate");

        // just a bounding box pass that will draw boundingboxes instead of the geometry itself
        m_GLTFBBox = new GltfBBoxPass();
            m_GLTFBBox->OnCreate(
            m_pDevice,
            m_RenderPassJustDepthAndHdr.GetRenderPass(),
            &m_ResourceViewHeaps,
            &m_ConstantBufferRing,
            &m_VidMemBufferPool,
            m_pGLTFTexturesAndBuffers,
            &m_Wireframe
        );

        // we are borrowing the upload heap command list for uploading to the GPU the IBs and VBs
        m_VidMemBufferPool.UploadData(m_UploadHeap.GetCommandList());

    }
    else if (Stage == 10)
    {
        Profile p("Flush");

        m_UploadHeap.FlushAndFinish();

        //once everything is uploaded we dont need the upload heaps anymore
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

    assert(m_shadowMapPool.size() == m_ShadowSRVPool.size());
    while (!m_shadowMapPool.empty())
    {
        m_shadowMapPool.back().ShadowMap.OnDestroy();
        vkDestroyFramebuffer(m_pDevice->GetDevice(), m_shadowMapPool.back().ShadowFrameBuffer, nullptr);
        vkDestroyImageView(m_pDevice->GetDevice(), m_ShadowSRVPool.back(), nullptr);
        vkDestroyImageView(m_pDevice->GetDevice(), m_shadowMapPool.back().ShadowDSV, nullptr);
        m_ShadowSRVPool.pop_back();
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
        std::vector<SceneShadowInfo>::iterator CurrentShadow = m_shadowMapPool.begin();
        for (uint32_t i = 0; CurrentShadow < m_shadowMapPool.end(); ++i, ++CurrentShadow)
        {
            CurrentShadow->ShadowMap.InitDepthStencil(m_pDevice, CurrentShadow->ShadowResolution, CurrentShadow->ShadowResolution, VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, "ShadowMap");
            CurrentShadow->ShadowMap.CreateDSV(&CurrentShadow->ShadowDSV);

            // Create render pass shadow, will clear contents
            {
                VkAttachmentDescription depthAttachments;
                AttachClearBeforeUse(VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, &depthAttachments);

                // Create frame buffer
                VkImageView attachmentViews[1] = { CurrentShadow->ShadowDSV };
                VkFramebufferCreateInfo fb_info = {};
                fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                fb_info.pNext = NULL;
                fb_info.renderPass = m_Render_pass_shadow;
                fb_info.attachmentCount = 1;
                fb_info.pAttachments = attachmentViews;
                fb_info.width = CurrentShadow->ShadowResolution;
                fb_info.height = CurrentShadow->ShadowResolution;
                fb_info.layers = 1;
                VkResult res = vkCreateFramebuffer(m_pDevice->GetDevice(), &fb_info, NULL, &CurrentShadow->ShadowFrameBuffer);
                assert(res == VK_SUCCESS);
            }

            VkImageView ShadowSRV;
            CurrentShadow->ShadowMap.CreateSRV(&ShadowSRV);
            m_ShadowSRVPool.push_back(ShadowSRV);
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

    // Wait for swapchain (we are going to render to it) -----------------------------------
    int imageIndex = pSwapChain->WaitForSwapChain();

    // Let our resource managers do some house keeping 
    m_ConstantBufferRing.OnBeginFrame();
    m_CommandListRing.OnBeginFrame();

    // command buffer calls
    VkCommandBuffer cmdBuf1 = m_CommandListRing.GetNewCommandList();

    {
        VkCommandBufferBeginInfo cmd_buf_info;
        cmd_buf_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cmd_buf_info.pNext = NULL;
        cmd_buf_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        cmd_buf_info.pInheritanceInfo = NULL;
        VkResult res = vkBeginCommandBuffer(cmdBuf1, &cmd_buf_info);
        assert(res == VK_SUCCESS);
    }

    m_GPUTimer.OnBeginFrame(cmdBuf1, &m_TimeStamps);

    // Render size viewport
    VkViewport vpr = { 0.0f, static_cast<float>(pState->renderHeight), static_cast<float>(pState->renderWidth), -static_cast<float>(pState->renderHeight), 0.0f, 1.0f };
    VkRect2D srr = { 0, 0, pState->renderWidth, pState->renderHeight };

    // Display size viewport
    VkViewport vpd = { 0.0f, static_cast<float>(pState->displayHeight), static_cast<float>(pState->displayWidth), -static_cast<float>(pState->displayHeight), 0.0f, 1.0f };
    VkRect2D srd = { 0, 0, pState->displayWidth, pState->displayHeight };

    m_pUpscaleContext->PreDraw(pState);

    float fLightMod = 1.f;
    // Sets the perFrame data 
    per_frame *pPerFrame = NULL;
    if (m_pGLTFTexturesAndBuffers)
    {
        // fill as much as possible using the GLTF (camera, lights, ...)
        pPerFrame = m_pGLTFTexturesAndBuffers->m_pGLTFCommon->SetPerFrameData(Cam);

        // Set some lighting factors
        pPerFrame->iblFactor = pState->IBLFactor;
        pPerFrame->emmisiveFactor = pState->EmissiveFactor;
        pPerFrame->invScreenResolution[0] = 1.0f / ((float)m_Width);
        pPerFrame->invScreenResolution[1] = 1.0f / ((float)m_Height);

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

    // Render all shadow maps
    if (m_GLTFDepth && pPerFrame != NULL)
    {
        SetPerfMarkerBegin(cmdBuf1, "ShadowPass");

        VkClearValue depth_clear_values[1];
        depth_clear_values[0].depthStencil.depth = 1.0f;
        depth_clear_values[0].depthStencil.stencil = 0;

        VkRenderPassBeginInfo rp_begin;
        rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rp_begin.pNext = NULL;
        rp_begin.renderPass = m_Render_pass_shadow;
        rp_begin.renderArea.offset.x = 0;
        rp_begin.renderArea.offset.y = 0;
        rp_begin.clearValueCount = 1;
        rp_begin.pClearValues = depth_clear_values;

        std::vector<SceneShadowInfo>::iterator ShadowMap = m_shadowMapPool.begin();
        while (ShadowMap < m_shadowMapPool.end())
        {
            // Clear shadow map
            rp_begin.framebuffer = ShadowMap->ShadowFrameBuffer;
            rp_begin.renderArea.extent.width = ShadowMap->ShadowResolution;
            rp_begin.renderArea.extent.height = ShadowMap->ShadowResolution;
            vkCmdBeginRenderPass(cmdBuf1, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

            // Render to shadow map
            SetViewportAndScissor(cmdBuf1, 0, 0, ShadowMap->ShadowResolution, ShadowMap->ShadowResolution);

            // Set per frame constant buffer values
            per_frame* cbDepthPerFrame = m_GLTFDepth->SetPerFrameConstants();
            cbDepthPerFrame->mCameraCurrViewProj = pPerFrame->lights[ShadowMap->LightIndex].mLightViewProj;
            cbDepthPerFrame->lodBias = pState->mipBias;

            m_GLTFDepth->Draw(cmdBuf1);

            m_GPUTimer.GetTimeStamp(cmdBuf1, "Shadow Map Render");

            vkCmdEndRenderPass(cmdBuf1);
            ++ShadowMap;
        }
        
        SetPerfMarkerEnd(cmdBuf1);
    }

    // Render Scene to the GBuffer ------------------------------------------------
    SetPerfMarkerBegin(cmdBuf1, "Color pass");

    VkRect2D currentRect = pState->bDynamicRes ? srd : srr;

    if (pPerFrame != NULL && m_GLTFPBR)
    {
        const bool bWireframe = pState->WireframeMode != UIState::WireframeMode::WIREFRAME_MODE_OFF;

        std::vector<GltfPbrPass::BatchList> opaque, transparent;
        m_GLTFPBR->BuildBatchLists(&opaque, &transparent, bWireframe);

        // Render opaque 
        {
            // Start the render pass with the display resolution so the entire render target is cleared when DRS is active
            m_RenderPassFullGBufferWithClear.BeginPass(cmdBuf1, currentRect);

            // After clearing set the correct render viewport/scissor rect
            vkCmdSetScissor(cmdBuf1, 0, 1, &srr);
            vkCmdSetViewport(cmdBuf1, 0, 1, &vpr);

            m_GLTFPBR->DrawBatchList(cmdBuf1, &opaque, bWireframe);

            if (pState->bRenderAnimatedTextures)
            {
                m_AnimatedTextures.Render(cmdBuf1, pState->m_bPlayAnimations ? (0.001f * (float)pState->deltaTime) : 0.0f, pState->m_fTextureAnimationSpeed, pState->bCompositionMask, Cam);
            }

            m_GPUTimer.GetTimeStamp(cmdBuf1, "PBR Opaque");

            m_RenderPassFullGBufferWithClear.EndPass(cmdBuf1);
        }

        // Render skydome
        {
            m_RenderPassJustDepthAndHdr.BeginPass(cmdBuf1, currentRect);

            vkCmdSetScissor(cmdBuf1, 0, 1, &srr);
            vkCmdSetViewport(cmdBuf1, 0, 1, &vpr);

            if (pState->SelectedSkydomeTypeIndex == 1)
            {
                math::Matrix4 clipToView = math::inverse(pPerFrame->mCameraCurrViewProj);
                m_SkyDome.Draw(cmdBuf1, clipToView);

                m_GPUTimer.GetTimeStamp(cmdBuf1, "Skydome cube");
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
                m_SkyDomeProc.Draw(cmdBuf1, skyDomeConstants);

                m_GPUTimer.GetTimeStamp(cmdBuf1, "Skydome Proc");
            }

            m_RenderPassJustDepthAndHdr.EndPass(cmdBuf1);
        }

        if (pState->nReactiveMaskMode == REACTIVE_MASK_MODE_AUTOGEN)
        {
            // Copy resource before we render transparent stuff
            {
                VkImageMemoryBarrier barrier = {};
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.pNext = NULL;
                barrier.srcAccessMask = 0;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                barrier.subresourceRange.baseMipLevel = 0;
                barrier.subresourceRange.levelCount = 1;
                barrier.subresourceRange.baseArrayLayer = 0;
                barrier.subresourceRange.layerCount = 1;

                VkImageMemoryBarrier barriers[2];
                barriers[0] = barrier;
                barriers[0].srcAccessMask = VK_IMAGE_LAYOUT_UNDEFINED;
                barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barriers[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;// VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                barriers[0].image = m_OpaqueTexture.Resource();

                barriers[1] = barrier;
                barriers[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                barriers[1].oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                barriers[1].image = m_GBuffer.m_HDR.Resource();

                vkCmdPipelineBarrier(cmdBuf1, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 2, barriers);
            }

            VkImageSubresourceLayers srLayers = {};
            srLayers.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            srLayers.baseArrayLayer = 0;
            srLayers.layerCount = 1;
            srLayers.mipLevel = 0;

            VkImageCopy region = {};
            region.extent.width = m_GBuffer.m_HDR.GetWidth();
            region.extent.height = m_GBuffer.m_HDR.GetHeight();
            region.extent.depth = 1;
            region.dstSubresource = srLayers;
            region.srcSubresource = srLayers;
            vkCmdCopyImage(cmdBuf1, m_GBuffer.m_HDR.Resource(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_OpaqueTexture.Resource(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

            // Copy resource before we render transparent stuff
            {
                VkImageMemoryBarrier barrier = {};
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.pNext = NULL;
                barrier.srcAccessMask = 0;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                barrier.subresourceRange.baseMipLevel = 0;
                barrier.subresourceRange.levelCount = 1;
                barrier.subresourceRange.baseArrayLayer = 0;
                barrier.subresourceRange.layerCount = 1;

                VkImageMemoryBarrier barriers[2];
                barriers[0] = barrier;
                barriers[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                barriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                barriers[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                barriers[0].image = m_OpaqueTexture.Resource();

                barriers[1] = barrier;
                barriers[1].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                barriers[1].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                barriers[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                barriers[1].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                barriers[1].image = m_GBuffer.m_HDR.Resource();

                vkCmdPipelineBarrier(cmdBuf1, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 2, barriers);
            }
        }

        // draw transparent geometry
        {
            if (pState->bRenderParticleSystem)
            {
                VkImageMemoryBarrier barrier = {};
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                barrier.subresourceRange.baseMipLevel = 0;
                barrier.subresourceRange.levelCount = 1;
                barrier.subresourceRange.baseArrayLayer = 0;
                barrier.subresourceRange.layerCount = 1;
                barrier.image = m_GBuffer.m_DepthBuffer.Resource();
                vkCmdPipelineBarrier(cmdBuf1, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

                m_pGPUParticleSystem->Render(cmdBuf1, m_ConstantBufferRing, m_state.flags, m_state.emitters, m_state.numEmitters, m_state.constantData);
            }

            m_RenderPassFullGBufferNoDepthWrite.BeginPass(cmdBuf1, currentRect);

            vkCmdSetScissor(cmdBuf1, 0, 1, &srr);
            vkCmdSetViewport(cmdBuf1, 0, 1, &vpr);

            std::sort(transparent.begin(), transparent.end());
            m_GLTFPBR->DrawBatchList(cmdBuf1, &transparent, bWireframe);
            m_GPUTimer.GetTimeStamp(cmdBuf1, "PBR Transparent");

            m_RenderPassFullGBufferNoDepthWrite.EndPass(cmdBuf1);
        }

        // draw object's bounding boxes
        {
            // Put the depth buffer back from the read only state to the write state
            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;
            barrier.image = m_GBuffer.m_DepthBuffer.Resource();
            vkCmdPipelineBarrier(cmdBuf1, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

            m_RenderPassJustDepthAndHdr.BeginPass(cmdBuf1, currentRect);

            vkCmdSetScissor(cmdBuf1, 0, 1, &srr);
            vkCmdSetViewport(cmdBuf1, 0, 1, &vpr);

            if (m_GLTFBBox)
            {
                if (pState->bDrawBoundingBoxes)
                {
                    m_GLTFBBox->Draw(cmdBuf1, pPerFrame->mCameraCurrViewProj);

                    m_GPUTimer.GetTimeStamp(cmdBuf1, "Bounding Box");
                }
            }

            // draw light's frustums
            if (pState->bDrawLightFrustum && pPerFrame != NULL)
            {
                SetPerfMarkerBegin(cmdBuf1, "light frustums");

                math::Vector4 vCenter = math::Vector4(0.0f, 0.0f, 0.5f, 0.0f);
                math::Vector4 vRadius = math::Vector4(1.0f, 1.0f, 0.5f, 0.0f);
                math::Vector4 vColor = math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
                for (uint32_t i = 0; i < pPerFrame->lightCount; i++)
                {
                    math::Matrix4 spotlightMatrix = math::inverse(pPerFrame->lights[i].mLightViewProj);
                    math::Matrix4 worldMatrix = pPerFrame->mCameraCurrViewProj * spotlightMatrix;
                    m_WireframeBox.Draw(cmdBuf1, &m_Wireframe, worldMatrix, vCenter, vRadius, vColor);
                }

                m_GPUTimer.GetTimeStamp(cmdBuf1, "Light's frustum");

                SetPerfMarkerEnd(cmdBuf1);
            }

            m_RenderPassJustDepthAndHdr.EndPass(cmdBuf1);
        }
    }
    else
    {
        m_RenderPassFullGBufferWithClear.BeginPass(cmdBuf1, currentRect);
        m_RenderPassFullGBufferWithClear.EndPass(cmdBuf1);
        m_RenderPassJustDepthAndHdr.BeginPass(cmdBuf1, currentRect);
        m_RenderPassJustDepthAndHdr.EndPass(cmdBuf1);
    }

    VkImageMemoryBarrier barrier[1] = {};
    barrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier[0].pNext = NULL;
    barrier[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier[0].oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier[0].subresourceRange.baseMipLevel = 0;
    barrier[0].subresourceRange.levelCount = 1;
    barrier[0].subresourceRange.baseArrayLayer = 0;
    barrier[0].subresourceRange.layerCount = 1;
    barrier[0].image = m_GBuffer.m_HDR.Resource();
    vkCmdPipelineBarrier(cmdBuf1, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, barrier);

    SetPerfMarkerEnd(cmdBuf1);

    // if FSR2 and auto reactive mask is enabled: generate reactive mask
    if (pState->nReactiveMaskMode == REACTIVE_MASK_MODE_AUTOGEN)
    {
        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.pNext = NULL;
        barrier.image = m_GBuffer.m_UpscaleReactive.Resource();
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(cmdBuf1, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

        UpscaleContext::FfxUpscaleSetup upscaleSetup = {};
        upscaleSetup.cameraSetup.vCameraPos = pState->camera.GetPosition();
        upscaleSetup.cameraSetup.mCameraView = pState->camera.GetView();
        upscaleSetup.cameraSetup.mCameraViewInv = math::inverse(pState->camera.GetView());
        upscaleSetup.cameraSetup.mCameraProj = pState->camera.GetProjection();
        
        upscaleSetup.opaqueOnlyColorResource = &m_OpaqueTexture;
        upscaleSetup.unresolvedColorResource = &m_GBuffer.m_HDR;
        upscaleSetup.reactiveMapResource = &m_GBuffer.m_UpscaleReactive;

        upscaleSetup.opaqueOnlyColorResourceView = m_OpaqueTextureSRV;
        upscaleSetup.unresolvedColorResourceView = m_GBuffer.m_HDRSRV;
        upscaleSetup.reactiveMapResourceView = m_GBuffer.m_UpscaleReactiveSRV;

        m_pUpscaleContext->GenerateReactiveMask(cmdBuf1, upscaleSetup, pState);
    }

    // Post proc---------------------------------------------------------------------------

    // Bloom, takes HDR as input and applies bloom to it.
    {
        SetPerfMarkerBegin(cmdBuf1, "PostProcess");
        
        // Downsample pass
        m_DownSample.Draw(cmdBuf1);
        m_GPUTimer.GetTimeStamp(cmdBuf1, "Downsample");

        // Bloom pass (needs the downsampled data)
        m_Bloom.Draw(cmdBuf1);
        m_GPUTimer.GetTimeStamp(cmdBuf1, "Bloom");

        SetPerfMarkerEnd(cmdBuf1);
    }

    // Start tracking input/output resources at this point to handle HDR and SDR render paths 
    VkImageView   SRVCurrentInput = m_GBuffer.m_HDRSRV;
    VkImage       ImgCurrentInput = m_GBuffer.m_HDR.Resource();
    VkFramebuffer CurrentFrambuffer = nullptr;

    // Always use upscale context, if we don't want one, Spatial can skip upscale (but still do TAA and/or RCAS)
    if (bUseUpscale || bUseTaaRcas)
    {
        {
            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.pNext = NULL;
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;

            VkImageMemoryBarrier barriers[6];
            barriers[0] = barrier;
            barriers[0].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            barriers[0].oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            barriers[0].image = m_GBuffer.m_DepthBuffer.Resource();

            barriers[1] = barrier;
            barriers[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            barriers[1].oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            barriers[1].image = m_GBuffer.m_MotionVectors.Resource();

            barriers[2] = barrier;
            barriers[2].srcAccessMask = pState->nReactiveMaskMode == REACTIVE_MASK_MODE_AUTOGEN ? VK_ACCESS_SHADER_WRITE_BIT : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            barriers[2].oldLayout = pState->nReactiveMaskMode == REACTIVE_MASK_MODE_AUTOGEN ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            barriers[2].image = m_GBuffer.m_UpscaleReactive.Resource();

            barriers[3] = barrier;
            barriers[3].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            barriers[3].oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            barriers[3].image = m_GBuffer.m_UpscaleTransparencyAndComposition.Resource();

            // no layout transition but we still need to wait
            barriers[4] = barrier;
            barriers[4].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barriers[4].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barriers[4].image = m_GBuffer.m_HDR.Resource();

            barriers[5] = barrier;
            barriers[5].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            barriers[5].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barriers[5].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barriers[5].newLayout = VK_IMAGE_LAYOUT_GENERAL;
            barriers[5].image = m_displayOutput.Resource();

            VkPipelineStageFlags srcStageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;

            if (pState->nReactiveMaskMode == REACTIVE_MASK_MODE_AUTOGEN)
                srcStageFlags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

            vkCmdPipelineBarrier(cmdBuf1, srcStageFlags, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 6, barriers);
        }

        UpscaleContext::FfxUpscaleSetup upscaleSetup;
        upscaleSetup.cameraSetup.vCameraPos = pState->camera.GetPosition();
        upscaleSetup.cameraSetup.mCameraView = pState->camera.GetView();
        upscaleSetup.cameraSetup.mCameraViewInv = math::inverse(pState->camera.GetView());
        upscaleSetup.cameraSetup.mCameraProj = pState->camera.GetProjection();

        upscaleSetup.unresolvedColorResource = &m_GBuffer.m_HDR;
        upscaleSetup.motionvectorResource = &m_GBuffer.m_MotionVectors;
        upscaleSetup.depthbufferResource = &m_GBuffer.m_DepthBuffer;
        upscaleSetup.reactiveMapResource = &m_GBuffer.m_UpscaleReactive;
        upscaleSetup.transparencyAndCompositionResource = &m_GBuffer.m_UpscaleTransparencyAndComposition;
        upscaleSetup.resolvedColorResource = &m_displayOutput;

        upscaleSetup.unresolvedColorResourceView = m_GBuffer.m_HDRSRV;
        upscaleSetup.motionvectorResourceView = m_GBuffer.m_MotionVectorsSRV;
        upscaleSetup.depthbufferResourceView = m_GBuffer.m_DepthBufferSRV;
        upscaleSetup.reactiveMapResourceView  = m_GBuffer.m_UpscaleReactiveSRV;
        upscaleSetup.transparencyAndCompositionResourceView = m_GBuffer.m_UpscaleTransparencyAndCompositionSRV;
        upscaleSetup.resolvedColorResourceView = m_displayOutputSRV;

        m_pUpscaleContext->Draw(cmdBuf1, upscaleSetup, pState);

        if (bUseUpscale)
            m_GPUTimer.GetTimeStamp(cmdBuf1, m_pUpscaleContext->Name().c_str());
        else if (pState->bUseTAA && !pState->bUseRcas)
            m_GPUTimer.GetTimeStamp(cmdBuf1, "TAA");
        else if (!pState->bUseTAA && pState->bUseRcas)
            m_GPUTimer.GetTimeStamp(cmdBuf1, "RCAS");
        else if (pState->bUseTAA && pState->bUseRcas)
            m_GPUTimer.GetTimeStamp(cmdBuf1, "TAA & RCAS");

        SRVCurrentInput = m_displayOutputSRV;
        ImgCurrentInput = m_displayOutput.Resource();
        CurrentFrambuffer = m_FramebufferDisplayOutput;

        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.pNext = NULL;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.image = ImgCurrentInput;

        vkCmdPipelineBarrier(cmdBuf1, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
    }

    // Upscale done, display size is now the main target
    vkCmdSetScissor(cmdBuf1, 0, 1, &srd);
    vkCmdSetViewport(cmdBuf1, 0, 1, &vpd);

    // If using FreeSync HDR we need to do the tonemapping in-place and then apply the GUI, later we'll apply the color conversion into the swapchain
    if (bHDR)
    {
        // Magnifier Pass: m_HDR as input, pass' own output
        if (pState->bUseMagnifier)
        {
            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.pNext = NULL;
            barrier.srcAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
            barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;
            barrier.image = m_MagnifierPS.GetPassOutputResource();

            if (m_bMagResourceReInit)
            {
                barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                
                m_bMagResourceReInit = false;
            }
            else
            {
                barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            }

            vkCmdPipelineBarrier(cmdBuf1, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

            // Note: assumes the input texture (specified in OnCreateWindowSizeDependentResources()) is in read state
            m_MagnifierPS.Draw(cmdBuf1, pState->MagnifierParams, SRVCurrentInput);
            m_GPUTimer.GetTimeStamp(cmdBuf1, "Magnifier");

            SRVCurrentInput = m_MagnifierPS.GetPassOutputSRV();
            ImgCurrentInput = m_MagnifierPS.GetPassOutputResource();
        }

        // Render HUD  ------------------------------------------------------------------------
        {
            VkRenderPassBeginInfo rp_begin;
            rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rp_begin.pNext = NULL;
            rp_begin.renderPass = m_RenderPassDisplayOutput;
            rp_begin.framebuffer = CurrentFrambuffer;
            rp_begin.renderArea = srd;
            rp_begin.pClearValues = nullptr;
            rp_begin.clearValueCount = 0;

            vkCmdBeginRenderPass(cmdBuf1, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

            vkCmdSetScissor(cmdBuf1, 0, 1, &srd);
            vkCmdSetViewport(cmdBuf1, 0, 1, &vpd);

            m_ImGUI.Draw(cmdBuf1);

            m_RenderPassJustDepthAndHdr.EndPass(cmdBuf1);

            m_GPUTimer.GetTimeStamp(cmdBuf1, "ImGUI Rendering");
        }

        // In place Tonemapping ------------------------------------------------------------------------
        {
            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.pNext = NULL;
            barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;
            barrier.image = ImgCurrentInput;
            vkCmdPipelineBarrier(cmdBuf1, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

            m_ToneMappingCS.Draw(cmdBuf1, SRVCurrentInput, pState->ExposureHdr, pState->SelectedTonemapperIndex, srd.extent.width, srd.extent.height);

            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.image = ImgCurrentInput;
            vkCmdPipelineBarrier(cmdBuf1, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
        }
    }

    // submit command buffer
    {
        VkResult res = vkEndCommandBuffer(cmdBuf1);
        assert(res == VK_SUCCESS);

        VkSubmitInfo submit_info;
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.pNext = NULL;
        submit_info.waitSemaphoreCount = 0;
        submit_info.pWaitSemaphores = NULL;
        submit_info.pWaitDstStageMask = NULL;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &cmdBuf1;
        submit_info.signalSemaphoreCount = 0;
        submit_info.pSignalSemaphores = NULL;
        res = vkQueueSubmit(m_pDevice->GetGraphicsQueue(), 1, &submit_info, VK_NULL_HANDLE);
        assert(res == VK_SUCCESS);
    }

    //m_CommandListRing.OnBeginFrame();

    VkCommandBuffer cmdBuf2 = m_CommandListRing.GetNewCommandList();

    {
        VkCommandBufferBeginInfo cmd_buf_info;
        cmd_buf_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cmd_buf_info.pNext = NULL;
        cmd_buf_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        cmd_buf_info.pInheritanceInfo = NULL;
        VkResult res = vkBeginCommandBuffer(cmdBuf2, &cmd_buf_info);
        assert(res == VK_SUCCESS);
    }

    vkCmdSetScissor(cmdBuf2, 0, 1, &srd);
    vkCmdSetViewport(cmdBuf2, 0, 1, &vpd);

    // Magnifier Pass: m_HDR as input, pass' own output
    if (pState->bUseMagnifier && !bHDR)
    {
        {
            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.pNext = NULL;
            barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;
            barrier.image = m_MagnifierPS.GetPassOutputResource();

            if (m_bMagResourceReInit)
            {
                barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;

                m_bMagResourceReInit = false;
            }
            else
            {
                barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            }

            vkCmdPipelineBarrier(cmdBuf2, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
        }

        // Note: assumes the input texture (specified in OnCreateWindowSizeDependentResources()) is in read state
        m_MagnifierPS.Draw(cmdBuf2, pState->MagnifierParams, SRVCurrentInput);
        m_GPUTimer.GetTimeStamp(cmdBuf2, "Magnifier");

        {
            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.pNext = NULL;
            barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;
            barrier.image = m_MagnifierPS.GetPassOutputResource();

            vkCmdPipelineBarrier(cmdBuf2, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
        }

        SRVCurrentInput = m_MagnifierPS.GetPassOutputSRV();
        ImgCurrentInput = m_MagnifierPS.GetPassOutputResource();
    }

    SetPerfMarkerBegin(cmdBuf2, "Swapchain RenderPass");

    // prepare render pass
    {
        VkRenderPassBeginInfo rp_begin = {};
        rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rp_begin.pNext = NULL;
        rp_begin.renderPass = pSwapChain->GetRenderPass();
        rp_begin.framebuffer = pSwapChain->GetFramebuffer(imageIndex);
        rp_begin.renderArea.offset.x = 0;
        rp_begin.renderArea.offset.y = 0;
        rp_begin.renderArea.extent = srd.extent;
        rp_begin.clearValueCount = 0;
        rp_begin.pClearValues = NULL;
        vkCmdBeginRenderPass(cmdBuf2, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);
    }

    vkCmdSetScissor(cmdBuf2, 0, 1, &srd);
    vkCmdSetViewport(cmdBuf2, 0, 1, &vpd);

    if (bHDR)
    {
        m_ColorConversionPS.Draw(cmdBuf2, SRVCurrentInput);
        m_GPUTimer.GetTimeStamp(cmdBuf2, "Color Conversion");
    }
    
    // For SDR pipeline, we apply the tonemapping and then draw the GUI and skip the color conversion
    else
    {
        // Tonemapping ------------------------------------------------------------------------
        {
            m_ToneMappingPS.Draw(cmdBuf2, SRVCurrentInput, pState->Exposure, pState->SelectedTonemapperIndex);
            m_GPUTimer.GetTimeStamp(cmdBuf2, "Tonemapping");
        }

        // Render HUD  -------------------------------------------------------------------------
        {
            m_ImGUI.Draw(cmdBuf2);
            m_GPUTimer.GetTimeStamp(cmdBuf2, "ImGUI Rendering");
        }
    }

    SetPerfMarkerEnd(cmdBuf2);

    vkCmdEndRenderPass(cmdBuf2);

    if (pState->bEnableFrameRateLimiter &&
        pState->bUseGPUFrameRateLimiter &&
        !m_TimeStamps.empty())
    {
        m_GpuFrameRateLimiter.Draw(cmdBuf2, &m_ConstantBufferRing, uint64_t(m_TimeStamps.back().m_microseconds), uint64_t(pState->targetFrameTime * 1000));
        m_GPUTimer.GetTimeStamp(cmdBuf2, "FrameRateLimiter");
    }

    m_GPUTimer.OnEndFrame();

    // Close & Submit the command list ----------------------------------------------------
    {
        VkResult res = vkEndCommandBuffer(cmdBuf2);
        assert(res == VK_SUCCESS);

        VkSemaphore ImageAvailableSemaphore;
        VkSemaphore RenderFinishedSemaphores;
        VkFence CmdBufExecutedFences;
        pSwapChain->GetSemaphores(&ImageAvailableSemaphore, &RenderFinishedSemaphores, &CmdBufExecutedFences);

        VkPipelineStageFlags submitWaitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submit_info2;
        submit_info2.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info2.pNext = NULL;
        submit_info2.waitSemaphoreCount = 1;
        submit_info2.pWaitSemaphores = &ImageAvailableSemaphore;
        submit_info2.pWaitDstStageMask = &submitWaitStage;
        submit_info2.commandBufferCount = 1;
        submit_info2.pCommandBuffers = &cmdBuf2;
        submit_info2.signalSemaphoreCount = 1;
        submit_info2.pSignalSemaphores = &RenderFinishedSemaphores;

        res = vkQueueSubmit(m_pDevice->GetGraphicsQueue(), 1, &submit_info2, CmdBufExecutedFences);
        assert(res == VK_SUCCESS);
    }
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
    else if (activeScene == 1) // Sponza
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