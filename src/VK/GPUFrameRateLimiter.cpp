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

void GPUFrameRateLimiter::OnCreate(Device* pDevice, DynamicBufferRing* pDynamicBufferRing, ResourceViewHeaps* pResourceViewHeaps)
{
    m_pDevice = pDevice;
    m_pResourceViewHeaps = pResourceViewHeaps;

    {
        std::vector<VkDescriptorSetLayoutBinding> layoutBindings(2);
        layoutBindings[0].binding = 0;
        layoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        layoutBindings[0].descriptorCount = 1;
        layoutBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        layoutBindings[1].binding = 1;
        layoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        layoutBindings[1].descriptorCount = 1;
        layoutBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        pResourceViewHeaps->CreateDescriptorSetLayout(&layoutBindings, &m_dsLayout);
        pResourceViewHeaps->AllocDescriptor(m_dsLayout, &m_ds);
    }

    m_pipeline.OnCreate(pDevice, "GPUFrameRateLimiter.hlsl", "CSMain", "-T cs_6_0", m_dsLayout, 32, 1, 1);

#ifdef USE_VMA
    VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferInfo.size = BufferLength * sizeof(float);
    bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocInfo.flags = VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT;

    VkResult res = vmaCreateBuffer(pDevice->GetAllocator(), &bufferInfo, &allocInfo, &m_buffer, &m_bufferAlloc, nullptr);
    assert(res == VK_SUCCESS);
    SetResourceName(pDevice->GetDevice(), VK_OBJECT_TYPE_BUFFER, (uint64_t)m_buffer, "FrameTimeLimiter (sys mem)");
#else
    // create the buffer, allocate it in SYSTEM memory, bind it and map it

    VkBufferCreateInfo buf_info = {};
    buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buf_info.pNext = NULL;
    buf_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    buf_info.size = BufferLength * sizeof(float);
    buf_info.queueFamilyIndexCount = 0;
    buf_info.pQueueFamilyIndices = NULL;
    buf_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    buf_info.flags = 0;
    res = vkCreateBuffer(m_pDevice->GetDevice(), &buf_info, NULL, &m_buffer);
    assert(res == VK_SUCCESS);

    // allocate the buffer in system memory

    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(m_pDevice->GetDevice(), m_buffer, &mem_reqs);

    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.pNext = NULL;
    alloc_info.memoryTypeIndex = VK_MEMORY_PROPERTY_DEVICE_LOCAL;
    alloc_info.allocationSize = mem_reqs.size;

    bool pass = memory_type_from_properties(m_pDevice->GetPhysicalDeviceMemoryProperties(), mem_reqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL,
        &alloc_info.memoryTypeIndex);
    assert(pass && "No mappable, coherent memory");

    res = vkAllocateMemory(m_pDevice->GetDevice(), &alloc_info, NULL, &m_deviceMemory);
    assert(res == VK_SUCCESS);

    // bind buffer

    res = vkBindBufferMemory(m_pDevice->GetDevice(), m_buffer, m_deviceMemory, 0);
    assert(res == VK_SUCCESS);
#endif        

    VkWriteDescriptorSet writeData;
    VkDescriptorBufferInfo bufferDescriptorInfo;

    bufferDescriptorInfo.buffer = m_buffer;
    bufferDescriptorInfo.offset = 0;
    bufferDescriptorInfo.range = BufferLength * sizeof(float);


    writeData = {};
    writeData.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeData.dstSet = m_ds;
    writeData.descriptorCount = 1;
    writeData.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writeData.pBufferInfo = &bufferDescriptorInfo;
    writeData.dstBinding = 0;
    writeData.dstArrayElement = 0;

    vkUpdateDescriptorSets(pDevice->GetDevice(), 1, &writeData, 0, nullptr);

    pDynamicBufferRing->SetDescriptorSet(1, sizeof(uint32_t), m_ds);

    m_frameTimeHistoryCount = 0;
    m_frameTimeHistorySum = 0;
}

void GPUFrameRateLimiter::OnDestroy()
{
    m_pipeline.OnDestroy();

    if (m_buffer != VK_NULL_HANDLE)
    {
#ifdef USE_VMA
        vmaDestroyBuffer(m_pDevice->GetAllocator(), m_buffer, m_bufferAlloc);
#else
        vkFreeMemory(m_pDevice->GetDevice(), m_deviceMemory, NULL);
        vkDestroyBuffer(m_pDevice->GetDevice(), m_buffer, NULL);
#endif
        m_buffer = VK_NULL_HANDLE;
    }

    if (m_dsLayout != VK_NULL_HANDLE)
        vkDestroyDescriptorSetLayout(m_pDevice->GetDevice(), m_dsLayout, nullptr);
}

void GPUFrameRateLimiter::Draw(VkCommandBuffer cmdBuf, DynamicBufferRing* pDynamicBufferRing, uint64_t lastFrameTimeMicrosecs, uint64_t targetFrameTimeMicrosecs)
{
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

    VkDescriptorBufferInfo dynamicUboInfo = pDynamicBufferRing->AllocConstantBuffer(sizeof(uint32_t), &numLoops);

    m_pipeline.Draw(cmdBuf, &dynamicUboInfo, m_ds, BufferLength / 32, 1, 1);
}