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
#include "../ffx-fsr2-api/vk/ffx_fsr2_vk.h"

static VkDeviceSize getMemoryUsageSnapshot(VkPhysicalDevice physicalDevice)
{
    // check if VK_EXT_memory_budget is enabled
    std::vector<VkExtensionProperties> extensionProperties;

    // enumerate all the device extensions 
    uint32_t deviceExtensionCount = 0;
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &deviceExtensionCount, nullptr);
    extensionProperties.resize(deviceExtensionCount);

    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &deviceExtensionCount, extensionProperties.data());

    bool extensionFound = false;

    for (uint32_t i = 0; i < deviceExtensionCount; i++)
    {
        if (strcmp(extensionProperties[i].extensionName, VK_EXT_MEMORY_BUDGET_EXTENSION_NAME) == 0)
            extensionFound = true;
    }

    if (!extensionFound)
        return 0;

    VkDeviceSize memoryUsage = 0;

    VkPhysicalDeviceMemoryBudgetPropertiesEXT memoryBudgetProperties = {};
    memoryBudgetProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;

    VkPhysicalDeviceMemoryProperties2 memoryProperties = {};
    memoryProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
    memoryProperties.pNext = &memoryBudgetProperties;

    vkGetPhysicalDeviceMemoryProperties2(physicalDevice, &memoryProperties);

    for (uint32_t i = 0; i < memoryProperties.memoryProperties.memoryTypeCount; i++)
        memoryUsage += memoryBudgetProperties.heapUsage[i];

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
    }
    else if (type == FFX_FSR2_MESSAGE_TYPE_WARNING)
    {
        OutputDebugStringW(L"FSR2_API_DEBUG_WARNING: ");
    }
    OutputDebugStringW(message);
    OutputDebugStringW(L"\n");
}

void UpscaleContext_FSR2_API::OnCreateWindowSizeDependentResources(
    VkImageView input, 
    VkImageView output, 
    uint32_t renderWidth, 
    uint32_t renderHeight, 
    uint32_t displayWidth, 
    uint32_t displayHeight,
    bool hdr)
{
    UpscaleContext::OnCreateWindowSizeDependentResources(input, output, renderWidth, renderHeight, displayWidth, displayHeight, hdr);

    // Setup VK interface.
    const size_t scratchBufferSize = ffxFsr2GetScratchMemorySizeVK(m_pDevice->GetPhysicalDevice());
    void* scratchBuffer = malloc(scratchBufferSize);
    FfxErrorCode errorCode = ffxFsr2GetInterfaceVK(&initializationParameters.callbacks, scratchBuffer, scratchBufferSize, m_pDevice->GetPhysicalDevice(), vkGetDeviceProcAddr);
    FFX_ASSERT(errorCode == FFX_OK);

    initializationParameters.device = ffxGetDeviceVK(m_pDevice->GetDevice());
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

    const uint64_t memoryUsageBefore = getMemoryUsageSnapshot(m_pDevice->GetPhysicalDevice());
    ffxFsr2ContextCreate(&context, &initializationParameters);
    const uint64_t memoryUsageAfter = getMemoryUsageSnapshot(m_pDevice->GetPhysicalDevice());
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

void UpscaleContext_FSR2_API::ReloadPipelines()
{
    m_pDevice->GPUFlush();
    OnDestroyWindowSizeDependentResources();
    OnCreateWindowSizeDependentResources(m_input, m_output, m_renderWidth, m_renderHeight, m_displayWidth, m_displayHeight, m_hdr);
}

void UpscaleContext_FSR2_API::BuildDevUI(UIState* pState)
{
    if (ImGui::Checkbox("Enable API Debug Checking", &m_enableDebugCheck))
    {
        ReloadPipelines();
    }

    pState->bReset = ImGui::Button("Reset accumulation");
}

void UpscaleContext_FSR2_API::GenerateReactiveMask(VkCommandBuffer pCommandList, const FfxUpscaleSetup& cameraSetup, UIState* pState)
{
    FfxFsr2GenerateReactiveDescription generateReactiveParameters;
    generateReactiveParameters.commandList = ffxGetCommandListVK(pCommandList);
    generateReactiveParameters.colorOpaqueOnly = ffxGetTextureResourceVK(&context, cameraSetup.opaqueOnlyColorResource->Resource(), cameraSetup.opaqueOnlyColorResourceView, cameraSetup.opaqueOnlyColorResource->GetWidth(), cameraSetup.opaqueOnlyColorResource->GetHeight(), cameraSetup.opaqueOnlyColorResource->GetFormat(), L"FSR2_OpaqueOnlyColorResource");
    generateReactiveParameters.colorPreUpscale = ffxGetTextureResourceVK(&context, cameraSetup.unresolvedColorResource->Resource(), cameraSetup.unresolvedColorResourceView, cameraSetup.unresolvedColorResource->GetWidth(), cameraSetup.unresolvedColorResource->GetHeight(), cameraSetup.unresolvedColorResource->GetFormat(), L"FSR2_UnresolvedColorResource");
    generateReactiveParameters.outReactive = ffxGetTextureResourceVK(&context, cameraSetup.reactiveMapResource->Resource(), cameraSetup.reactiveMapResourceView, cameraSetup.reactiveMapResource->GetWidth(), cameraSetup.reactiveMapResource->GetHeight(), cameraSetup.reactiveMapResource->GetFormat(), L"FSR2_InputReactiveMap", FFX_RESOURCE_STATE_GENERIC_READ);

    generateReactiveParameters.renderSize.width = pState->renderWidth;
    generateReactiveParameters.renderSize.height = pState->renderHeight;

    generateReactiveParameters.scale = pState->fFsr2AutoReactiveScale;
    generateReactiveParameters.cutoffThreshold = pState->fFsr2AutoReactiveThreshold;
    generateReactiveParameters.binaryValue = pState->fFsr2AutoReactiveBinaryValue;
    generateReactiveParameters.flags = (pState->bFsr2AutoReactiveTonemap ? FFX_FSR2_AUTOREACTIVEFLAGS_APPLY_TONEMAP : 0) |
        (pState->bFsr2AutoReactiveInverseTonemap ? FFX_FSR2_AUTOREACTIVEFLAGS_APPLY_INVERSETONEMAP : 0) |
        (pState->bFsr2AutoReactiveThreshold ? FFX_FSR2_AUTOREACTIVEFLAGS_APPLY_THRESHOLD : 0) |
        (pState->bFsr2AutoReactiveUseMax ? FFX_FSR2_AUTOREACTIVEFLAGS_USE_COMPONENTS_MAX : 0);

    ffxFsr2ContextGenerateReactiveMask(&context, &generateReactiveParameters);
}

void UpscaleContext_FSR2_API::Draw(VkCommandBuffer commandBuffer, const FfxUpscaleSetup& cameraSetup, UIState* pState)
{
    float farPlane = pState->camera.GetFarPlane();
    float nearPlane = pState->camera.GetNearPlane();

    if (m_bInvertedDepth)
    {
        // Cauldron1.0 can have planes inverted. Adjust before providing to FSR2.
        std::swap(farPlane, nearPlane);
    }

    FfxFsr2DispatchDescription dispatchParameters = {};
    dispatchParameters.commandList = ffxGetCommandListVK(commandBuffer);
    dispatchParameters.color = ffxGetTextureResourceVK(&context, cameraSetup.unresolvedColorResource->Resource(), cameraSetup.unresolvedColorResourceView, cameraSetup.unresolvedColorResource->GetWidth(), cameraSetup.unresolvedColorResource->GetHeight(), cameraSetup.unresolvedColorResource->GetFormat(), L"FSR2_InputColor");
    dispatchParameters.depth = ffxGetTextureResourceVK(&context, cameraSetup.depthbufferResource->Resource(), cameraSetup.depthbufferResourceView, cameraSetup.depthbufferResource->GetWidth(), cameraSetup.depthbufferResource->GetHeight(), cameraSetup.depthbufferResource->GetFormat(), L"FSR2_InputDepth");
    dispatchParameters.motionVectors = ffxGetTextureResourceVK(&context, cameraSetup.motionvectorResource->Resource(), cameraSetup.motionvectorResourceView, cameraSetup.motionvectorResource->GetWidth(), cameraSetup.motionvectorResource->GetHeight(), cameraSetup.motionvectorResource->GetFormat(), L"FSR2_InputMotionVectors");
    dispatchParameters.exposure = ffxGetTextureResourceVK(&context, nullptr, nullptr, 1, 1, VK_FORMAT_UNDEFINED, L"FSR2_InputExposure");

    if ((pState->nReactiveMaskMode == ReactiveMaskMode::REACTIVE_MASK_MODE_ON)
        || (pState->nReactiveMaskMode == ReactiveMaskMode::REACTIVE_MASK_MODE_AUTOGEN))
    {
        dispatchParameters.reactive = ffxGetTextureResourceVK(&context, cameraSetup.reactiveMapResource->Resource(), cameraSetup.reactiveMapResourceView, cameraSetup.reactiveMapResource->GetWidth(), cameraSetup.reactiveMapResource->GetHeight(), cameraSetup.reactiveMapResource->GetFormat(), L"FSR2_InputReactiveMap");
    }
    else
    {
        dispatchParameters.reactive = ffxGetTextureResourceVK(&context, nullptr, nullptr, 1, 1, VK_FORMAT_UNDEFINED, L"FSR2_EmptyInputReactiveMap");
    }

    if (pState->bCompositionMask == true)
    {
        dispatchParameters.transparencyAndComposition = ffxGetTextureResourceVK(&context, cameraSetup.transparencyAndCompositionResource->Resource(), cameraSetup.transparencyAndCompositionResourceView, cameraSetup.transparencyAndCompositionResource->GetWidth(), cameraSetup.transparencyAndCompositionResource->GetHeight(), cameraSetup.transparencyAndCompositionResource->GetFormat(), L"FSR2_TransparencyAndCompositionMap");
    }
    else
    {
        dispatchParameters.transparencyAndComposition = ffxGetTextureResourceVK(&context, nullptr, nullptr, 1, 1, VK_FORMAT_UNDEFINED, L"FSR2_EmptyTransparencyAndCompositionMap");
    }

    dispatchParameters.output = ffxGetTextureResourceVK(&context, cameraSetup.resolvedColorResource->Resource(), cameraSetup.resolvedColorResourceView, cameraSetup.resolvedColorResource->GetWidth(), cameraSetup.resolvedColorResource->GetHeight(), cameraSetup.resolvedColorResource->GetFormat(), L"FSR2_OutputUpscaledColor", FFX_RESOURCE_STATE_UNORDERED_ACCESS);
    dispatchParameters.jitterOffset.x = m_JitterX;
    dispatchParameters.jitterOffset.y = m_JitterY;
    dispatchParameters.motionVectorScale.x = (float)pState->renderWidth;
    dispatchParameters.motionVectorScale.y = (float)pState->renderHeight;
    dispatchParameters.reset = pState->bReset;
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