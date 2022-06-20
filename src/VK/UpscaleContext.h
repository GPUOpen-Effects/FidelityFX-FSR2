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
#include "Base/Texture.h"
#include "Base/StaticBufferPool.h"
#include "Base/GBuffer.h"
#include "PostProc/PostProcCS.h"
#include "UI.h"

using namespace CAULDRON_VK;
#define GROUP_SIZE  8

class UpscaleContext
{
public:
    typedef struct
    {
        UpscaleType         nType;
        bool                bInvertedDepth;
        Device* pDevice;
        VkFormat         outFormat;
        UploadHeap* pUploadHeap;
        uint32_t            maxQueuedFrames;
    }FfxUpscaleInitParams;

    typedef struct
    {
        math::Matrix4   mCameraView;
        math::Matrix4   mCameraProj;
        math::Matrix4   mCameraViewInv;
        math::Vector4   vCameraPos;
    }FfxCameraSetup;

    typedef struct
    {
        FfxCameraSetup  cameraSetup;

        Texture* unresolvedColorResource;               // input0
        Texture* motionvectorResource;                  // input1
        Texture* depthbufferResource;                   // input2
        Texture* reactiveMapResource;                   // input3
        Texture* transparencyAndCompositionResource;    // input4
        Texture* opaqueOnlyColorResource;               // input5
        Texture* resolvedColorResource;                 // output

        VkImageView unresolvedColorResourceView;            // input0
        VkImageView motionvectorResourceView;               // input1
        VkImageView depthbufferResourceView;                // input2
        VkImageView reactiveMapResourceView;                // input3
        VkImageView transparencyAndCompositionResourceView; // input4
        VkImageView opaqueOnlyColorResourceView;            // input5
        VkImageView resolvedColorResourceView;              // output
    }FfxUpscaleSetup;

    // factory
    static UpscaleContext* CreateUpscaleContext(const FfxUpscaleInitParams& initParams);


    UpscaleContext(std::string name);
    virtual UpscaleType         Type() { return m_Type; };
    virtual std::string         Name() { return "Upscale"; }

    virtual void                OnCreate(const FfxUpscaleInitParams& initParams);
    virtual void                OnDestroy();
    virtual void                OnCreateWindowSizeDependentResources(VkImageView input, VkImageView output, uint32_t renderWidth, uint32_t renderHeight, uint32_t displayWidth, uint32_t displayHeight, bool hdr);
    virtual void                OnDestroyWindowSizeDependentResources();
    virtual void                BuildDevUI(UIState* pState) {}
    virtual void                PreDraw(UIState* pState);
    virtual void                GenerateReactiveMask(VkCommandBuffer pCommandList, const FfxUpscaleSetup& cameraSetup, UIState* pState);
    virtual void                Draw(VkCommandBuffer commandBuffer, const FfxUpscaleSetup& cameraSetup, UIState* pState) = NULL;

protected:
    Device* m_pDevice;
    ResourceViewHeaps           m_ResourceViewHeaps;
    DynamicBufferRing           m_ConstantBufferRing;
    VkFormat                 m_OutputFormat;
    uint32_t                    m_MaxQueuedFrames;

    UpscaleType                 m_Type;
    std::string                 m_Name;

    bool                        m_bInvertedDepth;

    bool                        m_bUseTaa;
    uint32_t                    m_renderWidth, m_renderHeight;
    uint32_t                    m_displayWidth, m_displayHeight;
    bool                        m_bInit;

    uint32_t                    m_frameIndex;
    uint32_t                    m_index;
    float                       m_JitterX;
    float                       m_JitterY;

    VkImageView                 m_input;
    VkImageView                 m_output;
    bool                        m_hdr;
};