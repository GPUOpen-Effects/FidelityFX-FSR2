//
// Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
#pragma once

#include "../vk/stdafx.h"


namespace CAULDRON_VK
{

// For adding markers in RGP
class UserMarker
{
public:
    UserMarker(VkCommandBuffer commandBuffer, const char* name) : m_commandBuffer( commandBuffer ) { SetPerfMarkerBegin(m_commandBuffer, name); }
    ~UserMarker() { SetPerfMarkerEnd(m_commandBuffer); }

private:
    VkCommandBuffer  m_commandBuffer;
};


size_t FormatSize(VkFormat format);


class Buffer
{
public:
    Buffer() {}
    virtual ~Buffer() {}
    virtual void OnDestroy()
    {
        if (m_bufferView)
        {
            vkDestroyBufferView(m_pDevice->GetDevice(), m_bufferView, nullptr);
            m_bufferView = VK_NULL_HANDLE;
        }

        if (m_buffer)
        {
            vmaDestroyBuffer(m_pDevice->GetAllocator(), m_buffer, m_alloc);
            m_buffer = VK_NULL_HANDLE;
        }
        m_pDevice = nullptr;
        m_sizeInBytes = 0;
    }

    bool Init(Device *pDevice, int numElements, VkFormat format, const char* name)
    {
        m_pDevice = pDevice;
        m_sizeInBytes = numElements * FormatSize( format );
        VmaAllocationCreateInfo bufferAllocCreateInfo = {};
        bufferAllocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        bufferAllocCreateInfo.flags = VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT;
        bufferAllocCreateInfo.pUserData = (void*)name;
        VmaAllocationInfo gpuAllocInfo = {};

        VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        bufferInfo.size = m_sizeInBytes;
        bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;

        VkResult res = vmaCreateBuffer(m_pDevice->GetAllocator(), &bufferInfo, &bufferAllocCreateInfo, &m_buffer, &m_alloc, &gpuAllocInfo);
        assert(res == VK_SUCCESS);
        SetResourceName(pDevice->GetDevice(), VK_OBJECT_TYPE_BUFFER, (uint64_t)m_buffer, name);

        VkBufferViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
        viewInfo.format = format;
        viewInfo.buffer = m_buffer;
        viewInfo.range = m_sizeInBytes;
        vkCreateBufferView(pDevice->GetDevice(), &viewInfo, nullptr, &m_bufferView);
        SetResourceName(m_pDevice->GetDevice(), VK_OBJECT_TYPE_BUFFER_VIEW, (uint64_t)m_bufferView, name);
        return true;
    }

    bool Init(Device *pDevice, int numElements, size_t structSize, const char* name, bool indirectArgs)
    {
        m_pDevice = pDevice;
        m_sizeInBytes = numElements * structSize;
        VmaAllocationCreateInfo bufferAllocCreateInfo = {};
        bufferAllocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        bufferAllocCreateInfo.flags = VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT;
        bufferAllocCreateInfo.pUserData = (void*)name;
        VmaAllocationInfo gpuAllocInfo = {};

        VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        bufferInfo.size = m_sizeInBytes;
        bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        if ( indirectArgs )
            bufferInfo.usage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;

        VkResult res = vmaCreateBuffer(m_pDevice->GetAllocator(), &bufferInfo, &bufferAllocCreateInfo, &m_buffer, &m_alloc, &gpuAllocInfo);
        assert(res == VK_SUCCESS);
        SetResourceName(pDevice->GetDevice(), VK_OBJECT_TYPE_BUFFER, (uint64_t)m_buffer, name);

        return true;
    }

    VkBuffer& Resource() { return m_buffer; }

    void SetDescriptorSet(int index, VkDescriptorSet descriptorSet, bool asUAV) const
    {
        VkDescriptorBufferInfo descriptorBufferInfo = {};
        descriptorBufferInfo.buffer = m_buffer;
        descriptorBufferInfo.range = m_sizeInBytes;

        VkWriteDescriptorSet write = {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descriptorSet;
        write.descriptorCount = 1;
        if ( m_bufferView )
        {
            write.descriptorType = asUAV ? VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER : VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
            write.pTexelBufferView = &m_bufferView;
        }
        else
        {
            write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            write.pBufferInfo = &descriptorBufferInfo;
        }
        write.dstBinding = index;
        vkUpdateDescriptorSets(m_pDevice->GetDevice(), 1, &write, 0, nullptr);
    }

    void PipelineBarrier( VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask )
    {
        VkBufferMemoryBarrier memoryBarrier = {};
        memoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        memoryBarrier.dstAccessMask = dstStageMask == VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT ? VK_ACCESS_INDIRECT_COMMAND_READ_BIT : VK_ACCESS_SHADER_READ_BIT;
        memoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        memoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        memoryBarrier.buffer = m_buffer;
        memoryBarrier.size = m_sizeInBytes;
        vkCmdPipelineBarrier( commandBuffer, srcStageMask, dstStageMask, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 1, &memoryBarrier, 0, nullptr );
    }

    void AddPipelineBarrier( std::vector<VkBufferMemoryBarrier>& barrierList, VkPipelineStageFlags dstStageMask )
    {
        VkBufferMemoryBarrier memoryBarrier = {};
        memoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        memoryBarrier.dstAccessMask = dstStageMask == VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT ? VK_ACCESS_INDIRECT_COMMAND_READ_BIT : VK_ACCESS_SHADER_READ_BIT;
        memoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        memoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        memoryBarrier.buffer = m_buffer;
        memoryBarrier.size = m_sizeInBytes;
        barrierList.push_back( memoryBarrier );
    }

private:

    Device*                     m_pDevice = nullptr;
    VmaAllocation               m_alloc = VK_NULL_HANDLE;
    VkBuffer                    m_buffer = VK_NULL_HANDLE;
    size_t                      m_sizeInBytes = 0;
    VkBufferView                m_bufferView = VK_NULL_HANDLE;
};

}