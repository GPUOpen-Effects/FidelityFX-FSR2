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
#include "UpscaleContext_FSR2_API.h"
#include <d3dcompiler.h>
#include <d3d12shader.h>
#include "ShaderCompiler.h"
#include "../ffx-fsr2-api/dx12/ffx_fsr2_dx12.h"

static bool isLuidsEqual(LUID luid1, LUID luid2)
{
    return memcmp(&luid1, &luid2, sizeof(LUID)) == 0;
}

static uint64_t getMemoryUsageSnapshot(ID3D12Device* device)
{
    uint64_t memoryUsage = -1;
    IDXGIFactory* pFactory = nullptr;
    if (SUCCEEDED(CreateDXGIFactory2(0, IID_PPV_ARGS(&pFactory)))) {
        IDXGIAdapter* pAdapter = nullptr;
        UINT i = 0;
        while (pFactory->EnumAdapters(i++, &pAdapter) != DXGI_ERROR_NOT_FOUND)
        {
            DXGI_ADAPTER_DESC desc{};
            if (SUCCEEDED(pAdapter->GetDesc(&desc))) {
                if (isLuidsEqual(desc.AdapterLuid, device->GetAdapterLuid())) {
                    IDXGIAdapter4* pAdapter4 = nullptr;

                    if (SUCCEEDED(pAdapter->QueryInterface(IID_PPV_ARGS(&pAdapter4)))) {

                        DXGI_QUERY_VIDEO_MEMORY_INFO info{};
                        if (SUCCEEDED(pAdapter4->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info))) {
                            memoryUsage = info.CurrentUsage;
                        }

                        pAdapter4->Release();
                    }
                }

                pAdapter->Release();
            }
        }

        pFactory->Release();
    }

    return memoryUsage;
}

UpscaleContext_FSR2_API::UpscaleContext_FSR2_API(UpscaleType type, std::string name)
    : m_enableDebugCheck(false), UpscaleContext(name)
{
}

void UpscaleContext_FSR2_API::OnCreate(const FfxUpscaleInitParams& initParams)
{
    UpscaleContext::OnCreate(initParams);
}

void UpscaleContext_FSR2_API::OnDestroy()
{
    UpscaleContext::OnDestroy();
}

static void onFSR2Msg(FfxFsr2MsgType type, const wchar_t* message)
{
    if (type == FFX_FSR2_MESSAGE_TYPE_ERROR)
    {
        OutputDebugStringW(L"FSR2_API_DEBUG_ERROR: ");
    } else if (type == FFX_FSR2_MESSAGE_TYPE_WARNING)
    {
        OutputDebugStringW(L"FSR2_API_DEBUG_WARNING: ");
    }
    OutputDebugStringW(message);
    OutputDebugStringW(L"\n");
}

void UpscaleContext_FSR2_API::OnCreateWindowSizeDependentResources(
    ID3D12Resource* input,
    ID3D12Resource* output,
    uint32_t renderWidth,
    uint32_t renderHeight,
    uint32_t displayWidth,
    uint32_t displayHeight,
    bool hdr)
{
    UpscaleContext::OnCreateWindowSizeDependentResources(input, output, renderWidth, renderHeight, displayWidth, displayHeight, hdr);

    // Setup DX12 interface.
    const size_t scratchBufferSize = ffxFsr2GetScratchMemorySizeDX12();
    void* scratchBuffer = malloc(scratchBufferSize);
    FfxErrorCode errorCode = ffxFsr2GetInterfaceDX12(&initializationParameters.callbacks, m_pDevice->GetDevice(), scratchBuffer, scratchBufferSize);
    FFX_ASSERT(errorCode == FFX_OK);

    // This adds a ref to the device. The reference will get freed in ffxFsr2ContextDestroy
    initializationParameters.device = ffxGetDeviceDX12(m_pDevice->GetDevice());
    initializationParameters.maxRenderSize.width = renderWidth;
    initializationParameters.maxRenderSize.height = renderHeight;
    initializationParameters.displaySize.width = displayWidth;
    initializationParameters.displaySize.height = displayHeight;
    initializationParameters.flags = FFX_FSR2_ENABLE_AUTO_EXPOSURE;

    if (m_bInvertedDepth) {
        initializationParameters.flags |= FFX_FSR2_ENABLE_DEPTH_INVERTED | FFX_FSR2_ENABLE_DEPTH_INFINITE;
    }

    if (m_enableDebugCheck)
    {
        initializationParameters.flags |= FFX_FSR2_ENABLE_DEBUG_CHECKING;
        initializationParameters.fpMessage = &onFSR2Msg;
    }

    // Input data is HDR
    initializationParameters.flags |= FFX_FSR2_ENABLE_HIGH_DYNAMIC_RANGE;

#if COMPILE_FROM_HLSL
    // Override the shader creation so we can compile from HLSL source.
    FfxFsr2Interface d3dInterface = {};
    errorCode = ffxFsr2GetInterfaceDX12(&d3dInterface, m_pDevice->GetDevice(), scratchBuffer, scratchBufferSize);
    initializationParameters.callbacks.fpCreateRenderPass = s_bUseHLSLShaders ? createRenderPassFromSource : d3dInterface.fpCreateRenderPass;

    // copy for use by on-demand shader compile from HLSL
    memcpy(&s_initializationParameters, &initializationParameters, sizeof(FfxFsr2ContextDescription));
#endif // #if COMPILE_FROM_HLSL

    const uint64_t memoryUsageBefore = getMemoryUsageSnapshot(m_pDevice->GetDevice());
    ffxFsr2ContextCreate(&context, &initializationParameters);
    const uint64_t memoryUsageAfter = getMemoryUsageSnapshot(m_pDevice->GetDevice());
    memoryUsageInMegabytes = (memoryUsageAfter - memoryUsageBefore) * 0.000001f;
}

void UpscaleContext_FSR2_API::OnDestroyWindowSizeDependentResources()
{
    UpscaleContext::OnDestroyWindowSizeDependentResources();
    // only destroy contexts which are live
    if (initializationParameters.callbacks.scratchBuffer != nullptr)
    {
        ffxFsr2ContextDestroy(&context);
        free(initializationParameters.callbacks.scratchBuffer);
        initializationParameters.callbacks.scratchBuffer = nullptr;
    }
}

void UpscaleContext_FSR2_API::BuildDevUI(UIState* pState)
{
    if (ImGui::Checkbox("Enable API Debug Checking", &m_enableDebugCheck))
    {
        ReloadPipelines();
    }
}

void UpscaleContext_FSR2_API::ReloadPipelines()
{
    m_pDevice->GPUFlush();
    OnDestroyWindowSizeDependentResources();
    OnCreateWindowSizeDependentResources(m_input, m_output, m_renderWidth, m_renderHeight, m_displayWidth, m_displayHeight, m_hdr);
}

void UpscaleContext_FSR2_API::GenerateReactiveMask(ID3D12GraphicsCommandList* pCommandList, const FfxUpscaleSetup& cameraSetup, UIState* pState)
{
    FfxFsr2GenerateReactiveDescription generateReactiveParameters;
    generateReactiveParameters.commandList = ffxGetCommandListDX12(pCommandList);
    generateReactiveParameters.colorOpaqueOnly = ffxGetResourceDX12(&context, cameraSetup.opaqueOnlyColorResource);
    generateReactiveParameters.colorPreUpscale = ffxGetResourceDX12(&context, cameraSetup.unresolvedColorResource);
    generateReactiveParameters.outReactive = ffxGetResourceDX12(&context, cameraSetup.reactiveMapResource, L"FSR2_InputReactiveMap", FFX_RESOURCE_STATE_UNORDERED_ACCESS);

    generateReactiveParameters.renderSize.width = pState->renderWidth;
    generateReactiveParameters.renderSize.height = pState->renderHeight;

    generateReactiveParameters.scale = pState->fFsr2AutoReactiveScale;
    generateReactiveParameters.cutoffThreshold = pState->fFsr2AutoReactiveThreshold;
    generateReactiveParameters.binaryValue = pState->fFsr2AutoReactiveBinaryValue;
    generateReactiveParameters.flags =  (pState->bFsr2AutoReactiveTonemap ? FFX_FSR2_AUTOREACTIVEFLAGS_APPLY_TONEMAP :0) |
                                        (pState->bFsr2AutoReactiveInverseTonemap ? FFX_FSR2_AUTOREACTIVEFLAGS_APPLY_INVERSETONEMAP : 0) |
                                        (pState->bFsr2AutoReactiveThreshold ? FFX_FSR2_AUTOREACTIVEFLAGS_APPLY_THRESHOLD : 0) |
                                        (pState->bFsr2AutoReactiveUseMax ? FFX_FSR2_AUTOREACTIVEFLAGS_USE_COMPONENTS_MAX : 0);

    ffxFsr2ContextGenerateReactiveMask(&context, &generateReactiveParameters);
}

void UpscaleContext_FSR2_API::Draw(ID3D12GraphicsCommandList* pCommandList, const FfxUpscaleSetup& cameraSetup, UIState* pState)
{
    float farPlane = pState->camera.GetFarPlane();
    float nearPlane = pState->camera.GetNearPlane();

    if (m_bInvertedDepth)
    {
        // Cauldron1.0 can have planes inverted. Adjust before providing to FSR2.
        std::swap(farPlane, nearPlane);
    }

    FfxFsr2DispatchDescription dispatchParameters = {};
    dispatchParameters.commandList = ffxGetCommandListDX12(pCommandList);
    dispatchParameters.color = ffxGetResourceDX12(&context, cameraSetup.unresolvedColorResource, L"FSR2_InputColor");
    dispatchParameters.depth = ffxGetResourceDX12(&context, cameraSetup.depthbufferResource, L"FSR2_InputDepth");
    dispatchParameters.motionVectors = ffxGetResourceDX12(&context, cameraSetup.motionvectorResource, L"FSR2_InputMotionVectors");
    dispatchParameters.exposure = ffxGetResourceDX12(&context, nullptr, L"FSR2_InputExposure");

    if ((pState->nReactiveMaskMode == ReactiveMaskMode::REACTIVE_MASK_MODE_ON)
        || (pState->nReactiveMaskMode == ReactiveMaskMode::REACTIVE_MASK_MODE_AUTOGEN))
    {
        dispatchParameters.reactive = ffxGetResourceDX12(&context, cameraSetup.reactiveMapResource, L"FSR2_InputReactiveMap");
    }
    else
    {
        dispatchParameters.reactive = ffxGetResourceDX12(&context, nullptr, L"FSR2_EmptyInputReactiveMap");
    }

    if (pState->bCompositionMask == true)
    {
        dispatchParameters.transparencyAndComposition = ffxGetResourceDX12(&context, cameraSetup.transparencyAndCompositionResource, L"FSR2_TransparencyAndCompositionMap");
    }
    else
    {
        dispatchParameters.transparencyAndComposition = ffxGetResourceDX12(&context, nullptr, L"FSR2_EmptyTransparencyAndCompositionMap");
    }

    dispatchParameters.output = ffxGetResourceDX12(&context, cameraSetup.resolvedColorResource, L"FSR2_OutputUpscaledColor", FFX_RESOURCE_STATE_UNORDERED_ACCESS);
    dispatchParameters.jitterOffset.x = m_JitterX;
    dispatchParameters.jitterOffset.y = m_JitterY;
    dispatchParameters.motionVectorScale.x = (float)pState->renderWidth;
    dispatchParameters.motionVectorScale.y = (float)pState->renderHeight;
    dispatchParameters.reset = false;
    dispatchParameters.enableSharpening = pState->bUseRcas;
    dispatchParameters.sharpness = pState->sharpening;
    dispatchParameters.frameTimeDelta = (float)pState->deltaTime;
    dispatchParameters.preExposure = 1.0f;
    dispatchParameters.renderSize.width = pState->renderWidth;
    dispatchParameters.renderSize.height = pState->renderHeight;
    dispatchParameters.cameraFar = farPlane;
    dispatchParameters.cameraNear = nearPlane;
    dispatchParameters.cameraFovAngleVertical = pState->camera.GetFovV();
    pState->bReset = false;

    FfxErrorCode errorCode = ffxFsr2ContextDispatch(&context, &dispatchParameters);
    FFX_ASSERT(errorCode == FFX_OK);

}
