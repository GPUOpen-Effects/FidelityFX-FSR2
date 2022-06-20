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

#include "UpscaleContext.h"

using namespace CAULDRON_DX12;

class UpscaleContext_Spatial : public UpscaleContext
{
public:
                                UpscaleContext_Spatial(UpscaleType type, std::string name);

    virtual std::string         Name() { return "Spatial Upscale"; }
    virtual void                OnCreate(const FfxUpscaleInitParams& initParams);
    virtual void                OnDestroy();
    virtual void                OnCreateWindowSizeDependentResources(ID3D12Resource* input, ID3D12Resource* output, uint32_t renderWidth, uint32_t renderHeight, uint32_t displayWidth, uint32_t displayHeight, bool hdr);
    virtual void                OnDestroyWindowSizeDependentResources();
    virtual void                Draw(ID3D12GraphicsCommandList* pCommandList, const FfxUpscaleSetup& cameraSetup, UIState* pState);

protected:
    typedef struct _FfxTaaCB_
    {
        int                     g_PrevRenderResolution[2];
        int                     g_PrevScreenResolution[2];
        int                     g_CurRenderResolution[2];
        int                     g_CurScreenResolution[2];

        int                     g_taaMode;
        int                     g_ClosestVelocitySamplePattern;
        float                   g_FeedbackDefault;
        int                     _pad0;

        FfxCameraSetup          cameraCurr;
        FfxCameraSetup          cameraPrev;
    }FfxTaaCB;

    bool                        m_bUseRcas;
    bool                        m_bHdr;

    FfxTaaCB                    m_TaaConsts;

    PostProcCS                  m_upscale;
    PostProcCS                  m_upscaleTAA;           // Cauldron TAA requires Inverse Reinhard after upscale path
    PostProcCS                  m_rcas;
    PostProcCS                  m_taa;
    PostProcCS                  m_taaFirst;

    Texture                     m_TAAIntermediary[2];
    Texture                     m_FSRIntermediary;
    CBV_SRV_UAV_RING            m_TaaTableSrv;
    CBV_SRV_UAV_RING            m_TaaUav;
    CBV_SRV_UAV_RING            m_FsrSrv;
    CBV_SRV_UAV_RING            m_FsrUav;
    CBV_SRV_UAV_RING            m_RCasSrv;
    CBV_SRV_UAV_RING            m_RCasUav;

    bool                        m_bResetTaa = true;
};

