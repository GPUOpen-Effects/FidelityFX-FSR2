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


#include "stdafx.h"
#include "base/Texture.h"
#include "PostProc/PostProcCS.h"


class GPUFrameRateLimiter
{
public:
    void OnCreate(Device* pDevice, DynamicBufferRing* pDynamicBufferRing, ResourceViewHeaps* pResourceViewHeaps);
    void OnDestroy();
    void Draw(VkCommandBuffer cmd, DynamicBufferRing* pDynamicBufferRing, uint64_t lastFrameTimeMs, uint64_t targetFrameTimeMs);

private:
    Device* m_pDevice;
    ResourceViewHeaps* m_pResourceViewHeaps;
    PostProcCS      m_pipeline;
    VkBuffer        m_buffer;
    VkDescriptorSetLayout m_dsLayout;
    VkDescriptorSet m_ds;
#ifdef USE_VMA
    VmaAllocation    m_bufferAlloc = VK_NULL_HANDLE;
    VmaAllocation    m_bufferAllocVid = VK_NULL_HANDLE;
#else
    VkDeviceMemory   m_deviceMemory = VK_NULL_HANDLE;;
    VkDeviceMemory   m_deviceMemoryVid = VK_NULL_HANDLE;;
#endif

    double m_overhead = 1.0;

    static const uint32_t FRAME_TIME_HISTORY_SAMPLES = 4;
    uint64_t m_frameTimeHistory[FRAME_TIME_HISTORY_SAMPLES];
    uint64_t m_frameTimeHistorySum = 0;
    uint64_t m_frameTimeHistoryCount = 0;
};
