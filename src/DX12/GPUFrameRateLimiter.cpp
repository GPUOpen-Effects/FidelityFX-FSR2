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

#include "GPUFrameRateLimiter.h"

static const UINT BufferLength = 32768 * 32;

void GPUFrameRateLimiter::OnCreate(Device* pDevice, ResourceViewHeaps* pResourceViewHeaps)
{
    m_pipeline.OnCreate(pDevice, pResourceViewHeaps, "GPUFrameRateLimiter.hlsl", "CSMain", 1, 0, 32, 1, 1);

    const CD3DX12_RESOURCE_DESC bufDesc = CD3DX12_RESOURCE_DESC(
        D3D12_RESOURCE_DIMENSION_BUFFER, 0,
        BufferLength * sizeof(float), 1, 1, 1, DXGI_FORMAT_UNKNOWN,
        1, 0, D3D12_TEXTURE_LAYOUT_ROW_MAJOR, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    m_buffer.InitBuffer(pDevice, "FrameTimeLimiter", &bufDesc, sizeof(float), D3D12_RESOURCE_STATE_COMMON);
    m_bufferState = D3D12_RESOURCE_STATE_COMMON;

    pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(1, &m_UAVs);
    m_buffer.CreateBufferUAV(0, nullptr, &m_UAVs);

    m_frameTimeHistoryCount = 0;
    m_frameTimeHistorySum = 0;
}

void GPUFrameRateLimiter::OnDestroy()
{
    m_buffer.OnDestroy();
    m_pipeline.OnDestroy();
}

void GPUFrameRateLimiter::Draw(ID3D12GraphicsCommandList* pCmdList, DynamicBufferRing* pDynamicBufferRing, uint64_t lastFrameTimeMicrosecs, uint64_t targetFrameTimeMicrosecs)
{
    if (m_bufferState != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
    {
        pCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_buffer.GetResource(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
        m_bufferState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }

    constexpr double DampenFactor = 0.05;
    constexpr double MaxTargetFrameTimeUs = 200'000.0; // 200ms 5fps to match CPU limiter UI.
    constexpr double MinTargetFrameTimeUs = 50.0;

    if (m_frameTimeHistoryCount >= _countof(m_frameTimeHistory))
    {
        m_frameTimeHistorySum -= m_frameTimeHistory[m_frameTimeHistoryCount % _countof(m_frameTimeHistory)];
    }

    m_frameTimeHistorySum += lastFrameTimeMicrosecs;
    m_frameTimeHistory[m_frameTimeHistoryCount % _countof(m_frameTimeHistory)] = lastFrameTimeMicrosecs;
    m_frameTimeHistoryCount++;

    double recentFrameTimeAvg = double(m_frameTimeHistorySum) / min(m_frameTimeHistoryCount, _countof(m_frameTimeHistory));

    double clampedTargetFrameTimeMs = max(min(double(targetFrameTimeMicrosecs), MaxTargetFrameTimeUs), MinTargetFrameTimeUs);
    double deltaRatio = (recentFrameTimeAvg - clampedTargetFrameTimeMs) / clampedTargetFrameTimeMs;

    m_overhead -= m_overhead * deltaRatio * DampenFactor;
    m_overhead = min(max(1.0, m_overhead), 1000000.0);

    uint32_t numLoops = uint32_t(m_overhead);

    const D3D12_GPU_VIRTUAL_ADDRESS cbVa = pDynamicBufferRing->AllocConstantBuffer(sizeof(uint32_t), &numLoops);

    m_pipeline.Draw(pCmdList, cbVa, &m_UAVs, nullptr, BufferLength / 32, 1, 1);
}
