// FidelityFX Super Resolution Sample
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


class AnimatedTextures
{
public:

    AnimatedTextures() {}
    virtual ~AnimatedTextures() {}

    void OnCreate( Device& device, UploadHeap& uploadHeap, StaticBufferPool& bufferPool, VkRenderPass renderPass, ResourceViewHeaps& resourceViewHeaps, DynamicBufferRing& constantBufferRing );
    void OnDestroy();

    void Render( VkCommandBuffer commandList, float frameTime, float speed, bool compositionMask, const Camera& camera );

private:

    Device*                     m_pDevice = nullptr;
    DynamicBufferRing*          m_constantBufferRing = nullptr;

    VkDescriptorSetLayout       m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet             m_descriptorSets[3] = {};
    VkPipelineLayout            m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline                  m_pipelines[2] = {};
    VkDescriptorBufferInfo      m_indexBuffer = {};

    Texture                     m_textures[3] = {};
    VkImageView                 m_textureSRVs[3] = {};
    VkSampler                   m_sampler = VK_NULL_HANDLE;

    float                       m_scrollFactor = 0.0f;
    float                       m_rotationFactor = 0.0f;
    float                       m_flipTimer = 0.0f;
};