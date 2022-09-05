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

    // TAA descriptor tables
    m_TaaTableSrv.Init(&m_ResourceViewHeaps, 4, m_MaxQueuedFrames);
    m_TaaUav.Init(&m_ResourceViewHeaps, 1, m_MaxQueuedFrames);
    m_FsrSrv.Init(&m_ResourceViewHeaps, 1, m_MaxQueuedFrames);
    m_FsrUav.Init(&m_ResourceViewHeaps, 1, m_MaxQueuedFrames);
    m_RCasSrv.Init(&m_ResourceViewHeaps, 1, m_MaxQueuedFrames);
    m_RCasUav.Init(&m_ResourceViewHeaps, 1, m_MaxQueuedFrames);

    CD3DX12_STATIC_SAMPLER_DESC sd[4] = {};
    sd[0].Init(0, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
    sd[1].Init(1, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
    sd[2].Init(2, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
    sd[3].Init(3, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

    DefineList defines;
    defines["UPSCALE_TYPE"] = std::to_string(UPSCALE_TYPE_NATIVE);
    defines["SAMPLE_SLOW_FALLBACK"] = "1";
    defines["SAMPLE_EASU"] = "0";
    defines["SAMPLE_RCAS"] = "0";
    defines["USE_WS_MOTIONVECTORS"] = "0";
    m_taa.OnCreate(m_pDevice, &m_ResourceViewHeaps, "TAA.hlsl", "main", 1, 4, 16, 16, 1, &defines, 4, sd);
    m_taaFirst.OnCreate(m_pDevice, &m_ResourceViewHeaps, "TAA.hlsl", "first", 1, 4, 16, 16, 1, &defines, 4, sd);

    defines["SAMPLE_SLOW_FALLBACK"] = (slowFallback ? "1" : "0");
    defines["UPSCALE_TYPE"] = std::to_string(UPSCALE_TYPE_FSR_1_0);
    defines["SAMPLE_RCAS"] = "1";
    defines["DO_INVREINHARD"] = "0";
    m_rcas.OnCreate(m_pDevice, &m_ResourceViewHeaps, "UpscaleSpatial.hlsl", "upscalePassCS", 1, 1, 64, 1, 1, &defines, 2, sd);
    
    defines["UPSCALE_TYPE"] = std::to_string(m_Type);
    defines["SAMPLE_EASU"] = "1";
    defines["SAMPLE_RCAS"] = "0";
    m_upscale.OnCreate(m_pDevice, &m_ResourceViewHeaps, "UpscaleSpatial.hlsl", "upscalePassCS", 1, 1, 64, 1, 1, &defines, 2, sd);

    defines["DO_INVREINHARD"] = "1";
    m_upscaleTAA.OnCreate(m_pDevice, &m_ResourceViewHeaps, "UpscaleSpatial.hlsl", "upscalePassCS", 1, 1, 64, 1, 1, &defines, 2, sd);
}

//--------------------------------------------------------------------------------------
//
// OnDestroy
//
//--------------------------------------------------------------------------------------
void UpscaleContext_Spatial::OnDestroy()
{
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
void UpscaleContext_Spatial::OnCreateWindowSizeDependentResources(ID3D12Resource* input, ID3D12Resource* output, uint32_t renderWidth, uint32_t renderHeight, uint32_t displayWidth, uint32_t displayHeight, bool hdr)
{
    UpscaleContext::OnCreateWindowSizeDependentResources(input, output, renderWidth, renderHeight, displayWidth, displayHeight, hdr);

    DXGI_FORMAT fmt = DXGI_FORMAT_R16G16B16A16_FLOAT;
    m_TAAIntermediary[0].InitRenderTarget(m_pDevice, "TaaIntermediary0", &CD3DX12_RESOURCE_DESC::Tex2D(fmt, renderWidth, renderHeight, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    m_TAAIntermediary[1].InitRenderTarget(m_pDevice, "TaaIntermediary1", &CD3DX12_RESOURCE_DESC::Tex2D(fmt, renderWidth, renderHeight, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    fmt = (m_bHdr ? DXGI_FORMAT_R10G10B10A2_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM);
    m_FSRIntermediary.InitRenderTarget(m_pDevice, "FSRIntermediary", &CD3DX12_RESOURCE_DESC::Tex2D(fmt, displayWidth, displayHeight, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    m_bResetTaa = true;
}

//--------------------------------------------------------------------------------------
//
// OnDestroyWindowSizeDependentResources
//
//--------------------------------------------------------------------------------------
void UpscaleContext_Spatial::OnDestroyWindowSizeDependentResources()
{
    m_TAAIntermediary[0].OnDestroy();
    m_TAAIntermediary[1].OnDestroy();
    m_FSRIntermediary.OnDestroy();
}

//--------------------------------------------------------------------------------------
//
// Draw: executes the Spatial upscale (optionally with additional TAA and sharpening passes)
//
//--------------------------------------------------------------------------------------
void UpscaleContext_Spatial::Draw(ID3D12GraphicsCommandList* pCommandList, const FfxUpscaleSetup& cameraSetup, UIState* pState)
{
    ID3D12DescriptorHeap* pDescriptorHeaps[] = { m_ResourceViewHeaps.GetCBV_SRV_UAVHeap(), m_ResourceViewHeaps.GetSamplerHeap() };
    pCommandList->SetDescriptorHeaps(2, pDescriptorHeaps);

    ID3D12Resource* curInput = cameraSetup.unresolvedColorResource;
    ID3D12Resource* curOutput = cameraSetup.resolvedColorResource;

    m_bUseRcas = pState->bUseRcas;

    // TAA
    if (m_bUseTaa)
    {
        UserMarker marker(pCommandList, "TAA");
        pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_TAAIntermediary[(m_index +1) & 1].GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

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

        D3D12_GPU_VIRTUAL_ADDRESS cbHandle = {};
        uint32_t* pConstMem = 0;
        m_ConstantBufferRing.AllocConstantBuffer(sizeof(FfxTaaCB), (void**)&pConstMem, &cbHandle);
        memcpy(pConstMem, &m_TaaConsts, sizeof(FfxTaaCB));

        // set up SRVs
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Format = cameraSetup.unresolvedColorResource->GetDesc().Format;
        m_pDevice->GetDevice()->CreateShaderResourceView(cameraSetup.unresolvedColorResource, &srvDesc, m_TaaTableSrv.GetFrame(m_frameIndex).GetCPU(0));

        // Depth: change typeless to FLOAT for depth
        switch (cameraSetup.depthbufferResource->GetDesc().Format)
        {
            // Override TYPELESS resources to prevent device removed.
        case DXGI_FORMAT_D32_FLOAT:
        case DXGI_FORMAT_R32_TYPELESS: srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
            break;
        case DXGI_FORMAT_R16_TYPELESS: srvDesc.Format = DXGI_FORMAT_R16_FLOAT;
            break;
        case DXGI_FORMAT_D16_UNORM: srvDesc.Format = DXGI_FORMAT_R16_UNORM;
            break;
        default:
            srvDesc.Format = cameraSetup.depthbufferResource->GetDesc().Format;
            break;
        }
        m_pDevice->GetDevice()->CreateShaderResourceView(cameraSetup.depthbufferResource, &srvDesc, m_TaaTableSrv.GetFrame(m_frameIndex).GetCPU(1));

        srvDesc.Format = m_TAAIntermediary[m_index & 1].GetResource()->GetDesc().Format;
        m_pDevice->GetDevice()->CreateShaderResourceView(m_TAAIntermediary[m_index & 1].GetResource(), &srvDesc, m_TaaTableSrv.GetFrame(m_frameIndex).GetCPU(2));

        srvDesc.Format = cameraSetup.motionvectorResource->GetDesc().Format;
        m_pDevice->GetDevice()->CreateShaderResourceView(cameraSetup.motionvectorResource, &srvDesc, m_TaaTableSrv.GetFrame(m_frameIndex).GetCPU(3));
        
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Format = m_OutputFormat;
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        uavDesc.Format = m_TAAIntermediary[(m_index + 1) & 1].GetResource()->GetDesc().Format;
        m_pDevice->GetDevice()->CreateUnorderedAccessView(m_TAAIntermediary[(m_index+1) & 1].GetResource(), 0, &uavDesc, m_TaaUav.GetFrame(m_frameIndex).GetCPU(0));

        static const int threadGroupWorkRegionDim = 16;
        int dispatchX = (pState->renderWidth + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;
        int dispatchY = (pState->renderHeight + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;
        CBV_SRV_UAV currTaaUavs = m_TaaUav.GetFrame(m_frameIndex);
        CBV_SRV_UAV currTaaSrvs = m_TaaTableSrv.GetFrame(m_frameIndex);
        if (m_bResetTaa)
        {
            m_taaFirst.Draw(pCommandList, cbHandle, &currTaaUavs, &currTaaSrvs, dispatchX, dispatchY, 1);
            m_bResetTaa = false;
        }
        else
        {
            m_taa.Draw(pCommandList, cbHandle, &currTaaUavs, &currTaaSrvs, dispatchX, dispatchY, 1);
        }

        // set up taa target
        curInput = m_TAAIntermediary[(m_index+1) & 1].GetResource();
        pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_TAAIntermediary[(m_index + 1) & 1].GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
    }

    // Upscale
    {
        UserMarker marker(pCommandList, "Upscale");
        ID3D12Resource* pFsrInput = curInput;
        ID3D12Resource* pFsrOutput = nullptr;
        if (m_bUseRcas)
        {
            // write to intermediary surface
            pFsrOutput = m_FSRIntermediary.GetResource();
        }
        else
        {
            // output to final
            pFsrOutput = cameraSetup.resolvedColorResource;
        }

        D3D12_GPU_VIRTUAL_ADDRESS cbHandle = {};
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

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Format = pFsrInput->GetDesc().Format;
        m_pDevice->GetDevice()->CreateShaderResourceView(pFsrInput, &srvDesc, m_FsrSrv.GetFrame(m_frameIndex).GetCPU(0));

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Format = m_OutputFormat;
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        uavDesc.Format = pFsrOutput->GetDesc().Format;
        m_pDevice->GetDevice()->CreateUnorderedAccessView(pFsrOutput, 0, &uavDesc, m_FsrUav.GetFrame(m_frameIndex).GetCPU(0));

        // This value is the image region dimension that each thread group of the FSR shader operates on
        static const int threadGroupWorkRegionDim = 16;
        int dispatchX = (pState->displayWidth + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;
        int dispatchY = (pState->displayHeight + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;

        CBV_SRV_UAV currFsrUavs = m_FsrUav.GetFrame(m_frameIndex);
        CBV_SRV_UAV currFsrSrvs = m_FsrSrv.GetFrame(m_frameIndex);
        if (m_bUseTaa)
            m_upscaleTAA.Draw(pCommandList, cbHandle, &currFsrUavs, &currFsrSrvs, dispatchX, dispatchY, 1);
        else
            m_upscale.Draw(pCommandList, cbHandle, &currFsrUavs, &currFsrSrvs, dispatchX, dispatchY, 1);

        curInput = pFsrOutput;
    }

    // RCAS
    if (pState->bUseRcas)
    {
        pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(curInput, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

        UserMarker marker(pCommandList, "RCAS");
        D3D12_GPU_VIRTUAL_ADDRESS cbHandle = {};
        {
            FSRConstants consts = {};
            FsrRcasCon(reinterpret_cast<FfxUInt32*>(&consts.Const0), pState->sharpening);
            consts.Sample.x = (m_bHdr ? 1 : 0);
            uint32_t* pConstMem = 0;
            m_ConstantBufferRing.AllocConstantBuffer(sizeof(FSRConstants), (void**)&pConstMem, &cbHandle);
            memcpy(pConstMem, &consts, sizeof(FSRConstants));
        }

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Format = curInput->GetDesc().Format;
        m_pDevice->GetDevice()->CreateShaderResourceView(curInput, &srvDesc, m_RCasSrv.GetFrame(m_frameIndex).GetCPU(0));

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Format = m_OutputFormat;
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        uavDesc.Format = curOutput->GetDesc().Format;
        m_pDevice->GetDevice()->CreateUnorderedAccessView(curOutput, 0, &uavDesc, m_RCasUav.GetFrame(m_frameIndex).GetCPU(0));

        static const int threadGroupWorkRegionDim = 16;
        int dispatchX = (pState->displayWidth + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;
        int dispatchY = (pState->displayHeight + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;

        CBV_SRV_UAV currRCasUavs = m_RCasUav.GetFrame(m_frameIndex);
        CBV_SRV_UAV currRCasSrvs = m_RCasSrv.GetFrame(m_frameIndex);
        m_rcas.Draw(pCommandList, cbHandle, &currRCasUavs, &currRCasSrvs, dispatchX, dispatchY, 1);

        pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(curInput, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
    }
}