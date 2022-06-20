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

using namespace CAULDRON_DX12;
#define GROUP_SIZE  8

class UpscaleContext
{
public:
    typedef struct
    {
        UpscaleType         nType;
        bool                bInvertedDepth;
        Device*             pDevice;
        DXGI_FORMAT         outFormat;
        UploadHeap*         pUploadHeap;
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
        ID3D12Resource* opaqueOnlyColorResource;                // opaque only input
        ID3D12Resource* unresolvedColorResource;                // input0
        ID3D12Resource* motionvectorResource;                   // input1
        ID3D12Resource* depthbufferResource;                    // input2
        ID3D12Resource* reactiveMapResource;                    // input3
        ID3D12Resource* transparencyAndCompositionResource;     // input4
        ID3D12Resource* resolvedColorResource;                  // output
    }FfxUpscaleSetup;

    struct CBV_SRV_UAV_RING
    {
        bool Init(ResourceViewHeaps* pHeaps, uint32_t numDescriptorsPerFrame, uint32_t numBufferedFrames)
        {
            descriptorsPerFrame = numDescriptorsPerFrame;
            numFrames = numBufferedFrames;
            return pHeaps->AllocCBV_SRV_UAVDescriptor(numDescriptorsPerFrame * numBufferedFrames, &innerDescriptorTable);
        }

        CBV_SRV_UAV GetFrame(uint32_t frameIndex) const
        {
            CBV_SRV_UAV result = {};

            const uint32_t tableOffset = descriptorsPerFrame * (frameIndex % numFrames);

            result.SetResourceView(
                innerDescriptorTable.GetSize(),
                innerDescriptorTable.GetDescriptorSize(),
                innerDescriptorTable.GetCPU(tableOffset),
                innerDescriptorTable.GetGPU(tableOffset));

            return result;
        }

        CBV_SRV_UAV innerDescriptorTable;
        uint32_t descriptorsPerFrame = 0;
        uint32_t numFrames = 1;
    };

    // factory
    static UpscaleContext*      CreateUpscaleContext(const FfxUpscaleInitParams& initParams);
    

                                UpscaleContext(std::string name);
    virtual UpscaleType         Type() { return m_Type; };
    virtual std::string         Name() { return "Upscale"; }

    virtual void                OnCreate(const FfxUpscaleInitParams& initParams);
    virtual void                OnDestroy();
    virtual void                OnCreateWindowSizeDependentResources(
                                    ID3D12Resource* input,
                                    ID3D12Resource* output,
                                    uint32_t renderWidth,
                                    uint32_t renderHeight,
                                    uint32_t displayWidth,
                                    uint32_t displayHeight,
                                    bool hdr);
    virtual void                OnDestroyWindowSizeDependentResources();
    virtual void                BuildDevUI(UIState* pState) {}
    virtual void                PreDraw(UIState* pState);
    virtual void                GenerateReactiveMask(ID3D12GraphicsCommandList* pCommandList, const FfxUpscaleSetup& cameraSetup, UIState* pState);
    virtual void                Draw(ID3D12GraphicsCommandList* pCommandList, const FfxUpscaleSetup& cameraSetup, UIState* pState) = NULL;

protected:
    Device*                     m_pDevice;
    ResourceViewHeaps           m_ResourceViewHeaps;
    DynamicBufferRing           m_ConstantBufferRing;
    DXGI_FORMAT                 m_OutputFormat;
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

    ID3D12Resource*             m_input;
    ID3D12Resource*             m_output;
    bool                        m_hdr;
};

