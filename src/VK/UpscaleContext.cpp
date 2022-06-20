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

#include "stdafx.h"
#include "UpscaleContext.h"
#include "UpscaleContext_Spatial.h"
#include "UpscaleContext_FSR2_API.h"

#include "../ffx-fsr2-api/ffx_fsr2.h"

// factory
UpscaleContext* UpscaleContext::CreateUpscaleContext(const FfxUpscaleInitParams& initParams)
{
    UpscaleContext* pContext = NULL;

    switch (initParams.nType)
    {
    default:
    case UPSCALE_TYPE_NATIVE:
    case UPSCALE_TYPE_POINT:
    case UPSCALE_TYPE_BILINEAR:
    case UPSCALE_TYPE_BICUBIC:
    case UPSCALE_TYPE_FSR_1_0:
        pContext = new UpscaleContext_Spatial(initParams.nType, "Spatial");
        break;
    case UPSCALE_TYPE_FSR_2_0:
        pContext = new UpscaleContext_FSR2_API(initParams.nType, "FSR2 API");
        break;
        // Additional options:
        //		MSAA, MSAA resolve to higher resolution, then add TAA?
        //		Checkerboard?
    }

    pContext->OnCreate(initParams);

    return pContext;
}

UpscaleContext::UpscaleContext(std::string name)
    : m_Name(name)
{
}

//--------------------------------------------------------------------------------------
//
// OnCreate
//
//--------------------------------------------------------------------------------------
void UpscaleContext::OnCreate(const FfxUpscaleInitParams& initParams)
{
    m_bInvertedDepth = initParams.bInvertedDepth;
    m_pDevice = initParams.pDevice;
    m_OutputFormat = initParams.outFormat;
    m_Type = initParams.nType;
    m_MaxQueuedFrames = initParams.maxQueuedFrames;

    const uint32_t cbvDescriptorCount = 4000;
    const uint32_t srvDescriptorCount = 8000;
    const uint32_t uavDescriptorCount = 100;
    const uint32_t samplerDescriptorCount = 20;
    m_ResourceViewHeaps.OnCreate(m_pDevice, cbvDescriptorCount, srvDescriptorCount, uavDescriptorCount, samplerDescriptorCount);

    // Create a 'dynamic' constant buffer
    const uint32_t constantBuffersMemSize = 2 * 1024 * 1024;
    VkResult result = m_ConstantBufferRing.OnCreate(m_pDevice, 2, constantBuffersMemSize, "UpscaleContext_ConstantBufferRing");
    FFX_ASSERT(result == VK_SUCCESS);
}

//--------------------------------------------------------------------------------------
//
// OnDestroy
//
//--------------------------------------------------------------------------------------
void UpscaleContext::OnDestroy()
{
    m_ConstantBufferRing.OnDestroy();
    m_ResourceViewHeaps.OnDestroy();
}

//--------------------------------------------------------------------------------------
//
// OnCreateWindowSizeDependentResources
//
//--------------------------------------------------------------------------------------
void UpscaleContext::OnCreateWindowSizeDependentResources(VkImageView input, VkImageView output, uint32_t renderWidth, uint32_t renderHeight, uint32_t displayWidth, uint32_t displayHeight, bool hdr)
{
    m_input = input;
    m_output = output;
    m_renderWidth = renderWidth;
    m_renderHeight = renderHeight;
    m_displayWidth = displayWidth;
    m_displayHeight = displayHeight;
    m_index = 0;
    m_frameIndex = 0;
    m_bInit = true;
    m_hdr = hdr;
}

//--------------------------------------------------------------------------------------
//
// OnDestroyWindowSizeDependentResources
//
//--------------------------------------------------------------------------------------
void UpscaleContext::OnDestroyWindowSizeDependentResources()
{
}

//--------------------------------------------------------------------------------------
//
// PreDraw
//
//--------------------------------------------------------------------------------------
void UpscaleContext::PreDraw(UIState* pState)
{
    m_bUseTaa = pState->bUseTAA;

    m_ConstantBufferRing.OnBeginFrame();

    m_frameIndex++;

    if (m_bUseTaa)
    {
        m_index++;

        // handle reset and jitter
        const int32_t jitterPhaseCount = ffxFsr2GetJitterPhaseCount(pState->renderWidth, pState->displayWidth);
        ffxFsr2GetJitterOffset(&m_JitterX, &m_JitterY, m_index, jitterPhaseCount);

        pState->camera.SetProjectionJitter(2.0f * m_JitterX / (float)pState->renderWidth, -2.0f * m_JitterY / (float)pState->renderHeight);
    }
    else
    {
        pState->camera.SetProjectionJitter(0.f, 0.f);
    }
}

void UpscaleContext::GenerateReactiveMask(VkCommandBuffer pCommandList, const FfxUpscaleSetup& cameraSetup, UIState* pState)
{
}