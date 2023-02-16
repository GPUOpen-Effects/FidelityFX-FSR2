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

#include "../ffx-fsr2-api/ffx_fsr2.h"

using namespace CAULDRON_DX12;

class UpscaleContext_FSR2_API : public UpscaleContext
{
public:
    UpscaleContext_FSR2_API(UpscaleType type, std::string name);

    virtual std::string         Name() { return "FSR 2.0 API"; }
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
    virtual void                BuildDevUI(UIState* pState) override;
    virtual void                GenerateReactiveMask(ID3D12GraphicsCommandList* pCommandList, const FfxUpscaleSetup& cameraSetup, UIState* pState);
    virtual void                Draw(ID3D12GraphicsCommandList* pCommandList, const FfxUpscaleSetup& cameraSetup, UIState* pState);

private:
    void ReloadPipelines();

    FfxFsr2ContextDescription    initializationParameters = {};
    FfxFsr2Context              context;

    bool                        m_enableDebugCheck;
    float memoryUsageInMegabytes = 0;
};
