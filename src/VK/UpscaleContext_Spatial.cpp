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
#include "UpscaleContext_Spatial.h"

// CAS
#define FFX_CPU
#include "shaders/ffx_core.h"
#include "shaders/ffx_fsr1.h"


#include <DirectXMath.h>
using namespace DirectX;

struct FSRConstants
{
    XMUINT4 Const0;
    XMUINT4 Const1;
    XMUINT4 Const2;
    XMUINT4 Const3;
    XMUINT4 Sample;
};

// Upscaler using up to 3 passes:
// 1) optionally TAA
// 2) Spatial upscaler (point, bilinear, bicubic or EASU)
// 3) optionally RCAS

UpscaleContext_Spatial::UpscaleContext_Spatial(UpscaleType type, std::string name)
    : UpscaleContext(name)
{
    m_bUseRcas = true;
    m_bHdr = false;
}

//--------------------------------------------------------------------------------------
//
// OnCreate
//
//--------------------------------------------------------------------------------------
void UpscaleContext_Spatial::OnCreate(const FfxUpscaleInitParams& initParams)
{
    UpscaleContext::OnCreate(initParams);
    bool slowFallback = false;
    VkResult res;

    {
        // Create samplers
        VkSamplerCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        info.magFilter = VK_FILTER_NEAREST;
        info.minFilter = VK_FILTER_NEAREST;
        info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        info.minLod = -1000;
        info.maxLod = 1000;
        info.maxAnisotropy = 1.0f;

        res = vkCreateSampler(m_pDevice->GetDevice(), &info, NULL, &m_samplers[0]);
        assert(res == VK_SUCCESS);

        res = vkCreateSampler(m_pDevice->GetDevice(), &info, NULL, &m_samplers[1]);
        assert(res == VK_SUCCESS);

        res = vkCreateSampler(m_pDevice->GetDevice(), &info, NULL, &m_samplers[3]);
        assert(res == VK_SUCCESS);

        info.magFilter = VK_FILTER_LINEAR;
        info.minFilter = VK_FILTER_LINEAR;
        res = vkCreateSampler(m_pDevice->GetDevice(), &info, NULL, &m_samplers[4]);
        assert(res == VK_SUCCESS);

        info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        res = vkCreateSampler(m_pDevice->GetDevice(), &info, NULL, &m_samplers[2]);
        assert(res == VK_SUCCESS);
    }

    {
        // Create VkDescriptor Set Layout Bindings
        //

        std::vector<VkDescriptorSetLayoutBinding> m_TAAInputs(4 + 1 + 4);
        for (int i = 0; i < 4; i++)
        {
            m_TAAInputs[i].binding = i;
            m_TAAInputs[i].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            m_TAAInputs[i].descriptorCount = 1;
            m_TAAInputs[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            m_TAAInputs[i].pImmutableSamplers = NULL;
        }

        m_TAAInputs[4].binding = 4;
        m_TAAInputs[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        m_TAAInputs[4].descriptorCount = 1;
        m_TAAInputs[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        m_TAAInputs[4].pImmutableSamplers = NULL;

        for (int i = 5; i < 4 + 5; i++)
        {
            m_TAAInputs[i].binding = i;
            m_TAAInputs[i].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
            m_TAAInputs[i].descriptorCount = 1;
            m_TAAInputs[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            m_TAAInputs[i].pImmutableSamplers = &m_samplers[i - 5];
        }

        m_ResourceViewHeaps.CreateDescriptorSetLayout(&m_TAAInputs, &m_TaaDescriptorSetLayout);
        m_ResourceViewHeaps.AllocDescriptor(m_TaaDescriptorSetLayout, &m_TaaDescriptorSet[0]);
        m_ResourceViewHeaps.AllocDescriptor(m_TaaDescriptorSetLayout, &m_TaaDescriptorSet[1]);
        m_ResourceViewHeaps.AllocDescriptor(m_TaaDescriptorSetLayout, &m_TaaDescriptorSet[2]);

        DefineList defines;
        defines["UPSCALE_TYPE"] = std::to_string(UPSCALE_TYPE_NATIVE);
        defines["SAMPLE_SLOW_FALLBACK"] = "1";
        defines["SAMPLE_EASU"] = "0";
        defines["SAMPLE_RCAS"] = "0";
        defines["USE_WS_MOTIONVECTORS"] = "0";
        m_taa.OnCreate(m_pDevice, "TAA.hlsl", "main", "-T cs_6_0", m_TaaDescriptorSetLayout, 16, 16, 1, &defines);
        m_taaFirst.OnCreate(m_pDevice, "TAA.hlsl", "first", "-T cs_6_0", m_TaaDescriptorSetLayout, 16, 16, 1, &defines);
    }
    
    {
        std::vector<VkDescriptorSetLayoutBinding> layoutBindings(5);
        layoutBindings[0].binding = 0;
        layoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        layoutBindings[0].descriptorCount = 1;
        layoutBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        layoutBindings[0].pImmutableSamplers = nullptr;

        layoutBindings[1].binding = 1;
        layoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        layoutBindings[1].descriptorCount = 1;
        layoutBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        layoutBindings[1].pImmutableSamplers = nullptr;

        layoutBindings[2].binding = 2;
        layoutBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        layoutBindings[2].descriptorCount = 1;
        layoutBindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        layoutBindings[2].pImmutableSamplers = nullptr;

        layoutBindings[3].binding = 3;
        layoutBindings[3].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        layoutBindings[3].descriptorCount = 1;
        layoutBindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        layoutBindings[3].pImmutableSamplers = &m_samplers[0]; // Point

        layoutBindings[4].binding = 4;
        layoutBindings[4].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        layoutBindings[4].descriptorCount = 1;
        layoutBindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        layoutBindings[4].pImmutableSamplers = &m_samplers[4]; // Linear

        m_ResourceViewHeaps.CreateDescriptorSetLayout(&layoutBindings, &m_UpscaleDescriptorSetLayout);
        m_ResourceViewHeaps.AllocDescriptor(m_UpscaleDescriptorSetLayout, &m_UpscaleDescriptorSet[0]);
        m_ResourceViewHeaps.AllocDescriptor(m_UpscaleDescriptorSetLayout, &m_UpscaleDescriptorSet[1]);
        m_ResourceViewHeaps.AllocDescriptor(m_UpscaleDescriptorSetLayout, &m_UpscaleDescriptorSet[2]);

        DefineList defines;
        defines["SAMPLE_SLOW_FALLBACK"] = (slowFallback ? "1" : "0");
        defines["UPSCALE_TYPE"] = std::to_string(UPSCALE_TYPE_FSR_1_0);
        defines["SAMPLE_RCAS"] = "1";
        defines["DO_INVREINHARD"] = "0";
        m_rcas.OnCreate(m_pDevice, "UpscaleSpatial.hlsl", "upscalePassCS", "-T cs_6_0", m_UpscaleDescriptorSetLayout, 64, 1, 1, &defines);
        
        m_ResourceViewHeaps.AllocDescriptor(m_UpscaleDescriptorSetLayout, &m_RCASDescriptorSet[0]);
        m_ResourceViewHeaps.AllocDescriptor(m_UpscaleDescriptorSetLayout, &m_RCASDescriptorSet[1]);
        m_ResourceViewHeaps.AllocDescriptor(m_UpscaleDescriptorSetLayout, &m_RCASDescriptorSet[2]);

        defines["UPSCALE_TYPE"] = std::to_string(m_Type);
        defines["SAMPLE_EASU"] = "1";
        defines["SAMPLE_RCAS"] = "0";
        m_upscale.OnCreate(m_pDevice, "UpscaleSpatial.hlsl", "upscalePassCS", "-T cs_6_0", m_UpscaleDescriptorSetLayout, 64, 1, 1, &defines);

        defines["DO_INVREINHARD"] = "1";
        m_upscaleTAA.OnCreate(m_pDevice, "UpscaleSpatial.hlsl", "upscalePassCS", "-T cs_6_0", m_UpscaleDescriptorSetLayout, 64, 1, 1, &defines);
    }
}

//--------------------------------------------------------------------------------------
//
// OnDestroy
//
//--------------------------------------------------------------------------------------
void UpscaleContext_Spatial::OnDestroy()
{
    vkDestroySampler(m_pDevice->GetDevice(), m_samplers[0], nullptr);
    vkDestroySampler(m_pDevice->GetDevice(), m_samplers[1], nullptr);
    vkDestroySampler(m_pDevice->GetDevice(), m_samplers[2], nullptr);
    vkDestroySampler(m_pDevice->GetDevice(), m_samplers[3], nullptr);
    vkDestroySampler(m_pDevice->GetDevice(), m_samplers[4], nullptr);

    m_samplers[0] = nullptr;
    m_samplers[1] = nullptr;
    m_samplers[2] = nullptr;
    m_samplers[3] = nullptr;
    m_samplers[4] = nullptr;

    for (int i = 0; i < 3; i++)
    {
        m_ResourceViewHeaps.FreeDescriptor(m_TaaDescriptorSet[i]);
        m_ResourceViewHeaps.FreeDescriptor(m_UpscaleDescriptorSet[i]);
        m_ResourceViewHeaps.FreeDescriptor(m_RCASDescriptorSet[i]);

        m_TaaDescriptorSet[i] = nullptr;
        m_UpscaleDescriptorSet[i] = nullptr;
        m_RCASDescriptorSet[i] = nullptr;
    }

    vkDestroyDescriptorSetLayout(m_pDevice->GetDevice(), m_TaaDescriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(m_pDevice->GetDevice(), m_UpscaleDescriptorSetLayout, nullptr);

    m_TaaDescriptorSetLayout = nullptr;
    m_UpscaleDescriptorSetLayout = nullptr;

    m_taa.OnDestroy();
    m_taaFirst.OnDestroy();
    m_upscale.OnDestroy();
    m_upscaleTAA.OnDestroy();
    m_rcas.OnDestroy();

    UpscaleContext::OnDestroy();
}

//--------------------------------------------------------------------------------------
//
// OnCreateWindowSizeDependentResources
//
//--------------------------------------------------------------------------------------
void UpscaleContext_Spatial::OnCreateWindowSizeDependentResources(VkImageView input, VkImageView output, uint32_t renderWidth, uint32_t renderHeight, uint32_t displayWidth, uint32_t displayHeight, bool hdr)
{
    UpscaleContext::OnCreateWindowSizeDependentResources(input, output, renderWidth, renderHeight, displayWidth, displayHeight, hdr);

    // TAA buffers
    //
    m_TAAIntermediary[0].InitRenderTarget(m_pDevice, renderWidth, renderHeight, VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_1_BIT, (VkImageUsageFlags)(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT), false, "TaaIntermediary0");
    m_TAAIntermediary[0].CreateSRV(&m_TAAIntermediarySrv[0]);

    m_TAAIntermediary[1].InitRenderTarget(m_pDevice, renderWidth, renderHeight, VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_1_BIT, (VkImageUsageFlags)(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT), false, "TaaIntermediary1");
    m_TAAIntermediary[1].CreateSRV(&m_TAAIntermediarySrv[1]);

    // FSR buffer
    //
    m_FSRIntermediary.InitRenderTarget(m_pDevice, displayWidth, displayHeight, m_bHdr ? VK_FORMAT_A2R10G10B10_UNORM_PACK32 : VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_1_BIT, (VkImageUsageFlags)(VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT), false, "FSR Intermediary");
    m_FSRIntermediary.CreateSRV(&m_FSRIntermediarySrv);

    m_bResetTaa = true;
}

//--------------------------------------------------------------------------------------
//
// OnDestroyWindowSizeDependentResources
//
//--------------------------------------------------------------------------------------
void UpscaleContext_Spatial::OnDestroyWindowSizeDependentResources()
{
    vkDestroyImageView(m_pDevice->GetDevice(), m_TAAIntermediarySrv[0], 0);
    vkDestroyImageView(m_pDevice->GetDevice(), m_TAAIntermediarySrv[1], 0);
    vkDestroyImageView(m_pDevice->GetDevice(), m_FSRIntermediarySrv, 0);

    m_TAAIntermediarySrv[0] = nullptr;
    m_TAAIntermediarySrv[1] = nullptr;
    m_FSRIntermediarySrv = nullptr;

    m_TAAIntermediary[0].OnDestroy();
    m_TAAIntermediary[1].OnDestroy();
    m_FSRIntermediary.OnDestroy();
}

//--------------------------------------------------------------------------------------
//
// Draw: executes the Spatial upscale (optionally with additional TAA and sharpening passes)
//
//--------------------------------------------------------------------------------------
void UpscaleContext_Spatial::Draw(VkCommandBuffer commandBuffer, const FfxUpscaleSetup& cameraSetup, UIState* pState)
{
    VkImageView curInput = cameraSetup.unresolvedColorResourceView;
    VkImageView curOutput = cameraSetup.resolvedColorResourceView;

    m_bUseRcas = pState->bUseRcas;

    uint32_t currentDescriptorIndex = m_DescriptorSetIndex % 3;

    // TAA
    if (m_bUseTaa)
    {
        SetPerfMarkerBegin(commandBuffer, "TAA");

        {
            VkImageMemoryBarrier barriers[2] = {};
            barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barriers[0].pNext = NULL;
            barriers[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barriers[0].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barriers[0].oldLayout = m_bResetTaa ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barriers[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
            barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barriers[0].subresourceRange.baseMipLevel = 0;
            barriers[0].subresourceRange.levelCount = 1;
            barriers[0].subresourceRange.baseArrayLayer = 0;
            barriers[0].subresourceRange.layerCount = 1;
            barriers[0].image = m_TAAIntermediary[(m_index + 1) & 1].Resource();

            if (m_bResetTaa)
            {
                barriers[1] = barriers[0];
                barriers[1].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                barriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                barriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                barriers[1].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                barriers[1].image = m_TAAIntermediary[m_index & 1].Resource();
            }

            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, m_bResetTaa ? 2 : 1, barriers);
        }

        {
            // set up constantbuffer
            m_TaaConsts.g_PrevRenderResolution[0] = m_bInit ? m_renderWidth : m_TaaConsts.g_CurRenderResolution[0];
            m_TaaConsts.g_PrevRenderResolution[1] = m_bInit ? m_renderHeight : m_TaaConsts.g_CurRenderResolution[1];
            m_TaaConsts.g_PrevScreenResolution[0] = m_bInit ? m_displayWidth : m_TaaConsts.g_CurScreenResolution[0];
            m_TaaConsts.g_PrevScreenResolution[1] = m_bInit ? m_displayHeight : m_TaaConsts.g_CurScreenResolution[1];
            m_TaaConsts.g_CurRenderResolution[0] = pState->renderWidth;
            m_TaaConsts.g_CurRenderResolution[1] = pState->renderHeight;
            m_TaaConsts.g_CurScreenResolution[0] = m_displayWidth;
            m_TaaConsts.g_CurScreenResolution[1] = m_displayHeight;

            m_TaaConsts.g_ClosestVelocitySamplePattern = pState->closestVelocitySamplePattern;

            m_TaaConsts.g_FeedbackDefault = pState->Feedback;

            // set previous projection, remove jitter
            m_TaaConsts.cameraPrev = m_TaaConsts.cameraCurr;
            math::Vector4 projX = m_TaaConsts.cameraPrev.mCameraProj.getCol0();
            math::Vector4 projY = m_TaaConsts.cameraPrev.mCameraProj.getCol1();
            projX.setZ(0);
            projY.setZ(0);
            m_TaaConsts.cameraPrev.mCameraProj.setCol0(projX);
            m_TaaConsts.cameraPrev.mCameraProj.setCol1(projY);

            m_TaaConsts.cameraCurr.mCameraView = math::transpose(pState->camera.GetView());
            m_TaaConsts.cameraCurr.mCameraProj = math::transpose(pState->camera.GetProjection());
            m_TaaConsts.cameraCurr.mCameraViewInv = math::transpose(math::inverse(pState->camera.GetView()));
            m_TaaConsts.cameraCurr.vCameraPos = pState->camera.GetPosition();

            if (m_bInit) m_TaaConsts.cameraPrev = m_TaaConsts.cameraCurr;
        }

        {
            SetDescriptorSet(m_pDevice->GetDevice(), 0, cameraSetup.unresolvedColorResourceView, NULL, m_TaaDescriptorSet[currentDescriptorIndex]);
            SetDescriptorSet(m_pDevice->GetDevice(), 1, cameraSetup.depthbufferResourceView, NULL, m_TaaDescriptorSet[currentDescriptorIndex]);
            SetDescriptorSet(m_pDevice->GetDevice(), 2, m_TAAIntermediarySrv[m_index & 1], NULL, m_TaaDescriptorSet[currentDescriptorIndex]);
            SetDescriptorSet(m_pDevice->GetDevice(), 3, cameraSetup.motionvectorResourceView, NULL, m_TaaDescriptorSet[currentDescriptorIndex]);
            SetDescriptorSet(m_pDevice->GetDevice(), 4, m_TAAIntermediarySrv[(m_index + 1) & 1], m_TaaDescriptorSet[currentDescriptorIndex]);
        }

        static const int threadGroupWorkRegionDim = 16;
        int dispatchX = (pState->renderWidth + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;
        int dispatchY = (pState->renderHeight + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;
        if (m_bResetTaa)
        {
            m_taaFirst.Draw(commandBuffer, nullptr, m_TaaDescriptorSet[currentDescriptorIndex], dispatchX, dispatchY, 1);
            m_bResetTaa = false;
        }
        else
        {
            m_taa.Draw(commandBuffer, nullptr, m_TaaDescriptorSet[currentDescriptorIndex], dispatchX, dispatchY, 1);
        }

        // set up taa target
        curInput = m_TAAIntermediarySrv[(m_index + 1) & 1];

        {
            VkImageMemoryBarrier barriers[1] = {};
            barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barriers[0].pNext = NULL;
            barriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barriers[0].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            barriers[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barriers[0].subresourceRange.baseMipLevel = 0;
            barriers[0].subresourceRange.levelCount = 1;
            barriers[0].subresourceRange.baseArrayLayer = 0;
            barriers[0].subresourceRange.layerCount = 1;
            barriers[0].image = m_TAAIntermediary[(m_index + 1) & 1].Resource();

            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, barriers);
        }

        SetPerfMarkerEnd(commandBuffer);
    }

    // Upscale
    {
        SetPerfMarkerBegin(commandBuffer, "Upscale");
        VkImageView pFsrInput = curInput;
        VkImageView pFsrOutput = nullptr;

        if (m_bUseRcas)
        {
            // write to intermediary surface
            pFsrOutput = m_FSRIntermediarySrv;

            VkImageMemoryBarrier barriers[1] = {};
            barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barriers[0].pNext = NULL;
            barriers[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barriers[0].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barriers[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barriers[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
            barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barriers[0].subresourceRange.baseMipLevel = 0;
            barriers[0].subresourceRange.levelCount = 1;
            barriers[0].subresourceRange.baseArrayLayer = 0;
            barriers[0].subresourceRange.layerCount = 1;
            barriers[0].image = m_FSRIntermediary.Resource();

            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, barriers);
        }
        else
        {
            // output to final
            pFsrOutput = cameraSetup.resolvedColorResourceView;
        }

        VkDescriptorBufferInfo cbHandle = {};
        {
            FSRConstants consts = {};
            ffxFsrPopulateEasuConstants(
                reinterpret_cast<FfxUInt32*>(&consts.Const0),
                reinterpret_cast<FfxUInt32*>(&consts.Const1),
                reinterpret_cast<FfxUInt32*>(&consts.Const2),
                reinterpret_cast<FfxUInt32*>(&consts.Const3),
                static_cast<FfxFloat32>(pState->renderWidth), static_cast<FfxFloat32>(pState->renderHeight),
                static_cast<FfxFloat32>(m_renderWidth), static_cast<FfxFloat32>(m_renderHeight),
                static_cast<FfxFloat32>(pState->displayWidth), static_cast<FfxFloat32>(pState->displayHeight));
            uint32_t* pConstMem = 0;
            m_ConstantBufferRing.AllocConstantBuffer(sizeof(FSRConstants), (void**)&pConstMem, &cbHandle);
            memcpy(pConstMem, &consts, sizeof(FSRConstants));
        }

        {
            m_ConstantBufferRing.SetDescriptorSet(0, sizeof(FSRConstants), m_UpscaleDescriptorSet[currentDescriptorIndex]);
            SetDescriptorSet(m_pDevice->GetDevice(), 1, pFsrInput, NULL, m_UpscaleDescriptorSet[currentDescriptorIndex]);
            SetDescriptorSet(m_pDevice->GetDevice(), 2, pFsrOutput, m_UpscaleDescriptorSet[currentDescriptorIndex]);
        }

        // This value is the image region dimension that each thread group of the FSR shader operates on
        static const int threadGroupWorkRegionDim = 16;
        int dispatchX = (pState->displayWidth + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;
        int dispatchY = (pState->displayHeight + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;
        if (m_bUseTaa)
            m_upscaleTAA.Draw(commandBuffer, &cbHandle, m_UpscaleDescriptorSet[currentDescriptorIndex], dispatchX, dispatchY, 1);
        else
            m_upscale.Draw(commandBuffer, &cbHandle, m_UpscaleDescriptorSet[currentDescriptorIndex], dispatchX, dispatchY, 1);

        if (m_bUseRcas)
        {
            VkImageMemoryBarrier barriers[1] = {};
            barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barriers[0].pNext = NULL;
            barriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barriers[0].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            barriers[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barriers[0].subresourceRange.baseMipLevel = 0;
            barriers[0].subresourceRange.levelCount = 1;
            barriers[0].subresourceRange.baseArrayLayer = 0;
            barriers[0].subresourceRange.layerCount = 1;
            barriers[0].image = m_FSRIntermediary.Resource();

            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, barriers);
            curInput = pFsrOutput;
        }

        SetPerfMarkerEnd(commandBuffer);
    }

    // RCAS
    if (pState->bUseRcas)
    {
        SetPerfMarkerBegin(commandBuffer, "RCAS");

        VkDescriptorBufferInfo cbHandle = {};
        {
            FSRConstants consts = {};
            FsrRcasCon(reinterpret_cast<FfxUInt32*>(&consts.Const0), pState->sharpening);
            consts.Sample.x = (m_bHdr ? 1 : 0);
            uint32_t* pConstMem = 0;
            m_ConstantBufferRing.AllocConstantBuffer(sizeof(FSRConstants), (void**)&pConstMem, &cbHandle);
            memcpy(pConstMem, &consts, sizeof(FSRConstants));
        }

        {
            m_ConstantBufferRing.SetDescriptorSet(0, sizeof(FSRConstants), m_RCASDescriptorSet[currentDescriptorIndex]);
            SetDescriptorSet(m_pDevice->GetDevice(), 1, curInput, NULL, m_RCASDescriptorSet[currentDescriptorIndex]);
            SetDescriptorSet(m_pDevice->GetDevice(), 2, curOutput, m_RCASDescriptorSet[currentDescriptorIndex]);
        }

        static const int threadGroupWorkRegionDim = 16;
        int dispatchX = (pState->displayWidth + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;
        int dispatchY = (pState->displayHeight + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;

        m_rcas.Draw(commandBuffer, &cbHandle, m_RCASDescriptorSet[currentDescriptorIndex], dispatchX, dispatchY, 1);

        SetPerfMarkerEnd(commandBuffer);
    }

    m_DescriptorSetIndex++;
}