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


#include "AnimatedTexture.h"


struct ConstantBuffer
{
    math::Matrix4 currentViewProj;
    math::Matrix4 previousViewProj;
    float jitterCompensation[ 2 ];
    float scrollFactor;
    float rotationFactor;
    int mode;
    int pads[3];
};


void AnimatedTextures::OnCreate( Device& device, UploadHeap& uploadHeap, StaticBufferPool& bufferPool, VkRenderPass renderPass, ResourceViewHeaps& resourceViewHeaps, DynamicBufferRing& constantBufferRing )
{
    m_pDevice = &device;
    m_constantBufferRing = &constantBufferRing;

    VkSamplerCreateInfo sampler = {};
    sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler.magFilter = VK_FILTER_LINEAR;
    sampler.minFilter = VK_FILTER_LINEAR;
    sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler.minLod = -1000;
    sampler.maxLod = 1000;
    sampler.maxAnisotropy = 16.0f;
    VkResult res = vkCreateSampler( device.GetDevice(), &sampler, nullptr, &m_sampler);
    assert(res == VK_SUCCESS);

    // Compile shaders
    //
    DefineList attributeDefines;

    VkPipelineShaderStageCreateInfo vertexShader;
    res = VKCompileFromFile(m_pDevice->GetDevice(), VK_SHADER_STAGE_VERTEX_BIT, "AnimatedTexture.hlsl", "VSMain", "-T vs_6_0", &attributeDefines, &vertexShader);
    assert(res == VK_SUCCESS);

    VkPipelineShaderStageCreateInfo fragmentShader;
    res = VKCompileFromFile(m_pDevice->GetDevice(), VK_SHADER_STAGE_FRAGMENT_BIT, "AnimatedTexture.hlsl", "PSMain", "-T ps_6_0", &attributeDefines, &fragmentShader);
    assert(res == VK_SUCCESS);

    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    shaderStages.push_back(vertexShader);
    shaderStages.push_back(fragmentShader);

    std::vector<VkDescriptorSetLayoutBinding> layoutBindings(3);
    layoutBindings[0].binding = 0;
    layoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    layoutBindings[0].descriptorCount = 1;
    layoutBindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutBindings[0].pImmutableSamplers = nullptr;

    layoutBindings[1].binding = 1;
    layoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    layoutBindings[1].descriptorCount = 1;
    layoutBindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutBindings[1].pImmutableSamplers = nullptr;

    layoutBindings[2].binding = 2;
    layoutBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    layoutBindings[2].descriptorCount = 1;
    layoutBindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutBindings[2].pImmutableSamplers = &m_sampler;

    for (int i = 0; i < _countof(m_descriptorSets);i++)
    {
        resourceViewHeaps.CreateDescriptorSetLayoutAndAllocDescriptorSet( &layoutBindings, &m_descriptorSetLayout, &m_descriptorSets[i] );
        constantBufferRing.SetDescriptorSet( 0, sizeof( ConstantBuffer ), m_descriptorSets[i] );
    }

    // Create pipeline layout
    //
    VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = {};
    pPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pPipelineLayoutCreateInfo.setLayoutCount = 1;
    pPipelineLayoutCreateInfo.pSetLayouts = &m_descriptorSetLayout;

    res = vkCreatePipelineLayout(m_pDevice->GetDevice(), &pPipelineLayoutCreateInfo, NULL, &m_pipelineLayout);
    assert(res == VK_SUCCESS);

    VkPipelineVertexInputStateCreateInfo vi = {};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo ia = {};
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // rasterizer state
    VkPipelineRasterizationStateCreateInfo rs = {};
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1.0f;

    VkPipelineColorBlendAttachmentState att_state[4] = {};
    att_state[0].colorWriteMask = 0xf;
    att_state[0].blendEnable = VK_FALSE;
    att_state[0].alphaBlendOp = VK_BLEND_OP_ADD;
    att_state[0].colorBlendOp = VK_BLEND_OP_ADD;
    att_state[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    att_state[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    att_state[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    att_state[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;

    att_state[1].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT;
    att_state[2].colorWriteMask = 0x0;
    att_state[3].colorWriteMask = VK_COLOR_COMPONENT_R_BIT;

    // Color blend state

    VkPipelineColorBlendStateCreateInfo cb = {};
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = _countof(att_state);
    cb.pAttachments = att_state;
    cb.logicOpEnable = VK_FALSE;
    cb.logicOp = VK_LOGIC_OP_NO_OP;
    cb.blendConstants[0] = 1.0f;
    cb.blendConstants[1] = 1.0f;
    cb.blendConstants[2] = 1.0f;
    cb.blendConstants[3] = 1.0f;

    std::vector<VkDynamicState> dynamicStateEnables = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_BLEND_CONSTANTS
    };
    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.pNext = NULL;
    dynamicState.pDynamicStates = dynamicStateEnables.data();
    dynamicState.dynamicStateCount = (uint32_t)dynamicStateEnables.size();

    // view port state

    VkPipelineViewportStateCreateInfo vp = {};
    vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.scissorCount = 1;

    // depth stencil state

    VkPipelineDepthStencilStateCreateInfo ds = {};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = VK_TRUE;
    ds.depthWriteEnable = VK_TRUE;
    ds.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
    ds.back.failOp = VK_STENCIL_OP_KEEP;
    ds.back.passOp = VK_STENCIL_OP_KEEP;
    ds.back.compareOp = VK_COMPARE_OP_ALWAYS;
    ds.back.depthFailOp = VK_STENCIL_OP_KEEP;
    ds.front = ds.back;

    // multi sample state

    VkPipelineMultisampleStateCreateInfo ms = {};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // create pipeline 

    VkGraphicsPipelineCreateInfo pipeline = {};
    pipeline.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline.layout = m_pipelineLayout;
    pipeline.pVertexInputState = &vi;
    pipeline.pInputAssemblyState = &ia;
    pipeline.pRasterizationState = &rs;
    pipeline.pColorBlendState = &cb;
    pipeline.pMultisampleState = &ms;
    pipeline.pDynamicState = &dynamicState;
    pipeline.pViewportState = &vp;
    pipeline.pDepthStencilState = &ds;
    pipeline.pStages = shaderStages.data();
    pipeline.stageCount = (uint32_t)shaderStages.size();
    pipeline.renderPass = renderPass;
    pipeline.subpass = 0;

    res = vkCreateGraphicsPipelines(m_pDevice->GetDevice(), device.GetPipelineCache(), 1, &pipeline, NULL, &m_pipelines[0]);
    assert(res == VK_SUCCESS);
    SetResourceName(m_pDevice->GetDevice(), VK_OBJECT_TYPE_PIPELINE, (uint64_t)m_pipelines[0], "AT pipeline with comp");

    att_state[3].colorWriteMask = 0;
    res = vkCreateGraphicsPipelines(m_pDevice->GetDevice(), device.GetPipelineCache(), 1, &pipeline, NULL, &m_pipelines[1]);
    assert(res == VK_SUCCESS);
    SetResourceName(m_pDevice->GetDevice(), VK_OBJECT_TYPE_PIPELINE, (uint64_t)m_pipelines[1], "AT pipeline no comp");

    UINT indices[6] = { 0, 1, 2, 2, 1, 3 };
    bufferPool.AllocBuffer( _countof( indices ), sizeof( UINT ), indices, &m_indexBuffer );

    m_textures[0].InitFromFile( &device, &uploadHeap, "..\\media\\lion.jpg", true );
    m_textures[1].InitFromFile( &device, &uploadHeap, "..\\media\\checkerboard.dds", true );
    m_textures[2].InitFromFile( &device, &uploadHeap, "..\\media\\composition_text.dds", true );

    for ( int i = 0; i < _countof( m_textures ); i++ )
    {
        m_textures[ i ].CreateSRV( &m_textureSRVs[i] );
        SetDescriptorSet( m_pDevice->GetDevice(), 1, m_textureSRVs[i], nullptr, m_descriptorSets[i] );
    }
}


void AnimatedTextures::OnDestroy()
{
    vkDestroySampler(m_pDevice->GetDevice(), m_sampler, nullptr);
    m_sampler = VK_NULL_HANDLE;

    for ( int i = 0; i < _countof( m_textures ); i++ )
    {
        vkDestroyImageView(m_pDevice->GetDevice(), m_textureSRVs[i], nullptr);
        m_textureSRVs[i] = VK_NULL_HANDLE;

        m_textures[i].OnDestroy();
    }

    for ( int i = 0; i < _countof( m_pipelines ); i++ )
    {
        vkDestroyPipeline( m_pDevice->GetDevice(), m_pipelines[i], nullptr );
        m_pipelines[i] = VK_NULL_HANDLE;
    }

    vkDestroyPipelineLayout( m_pDevice->GetDevice(), m_pipelineLayout, nullptr );
    m_pipelineLayout = VK_NULL_HANDLE;

    vkDestroyDescriptorSetLayout( m_pDevice->GetDevice(), m_descriptorSetLayout, nullptr );
    m_descriptorSetLayout = VK_NULL_HANDLE;

    m_pDevice = nullptr;
}


void AnimatedTextures::Render( VkCommandBuffer commandList, float frameTime, float speed, bool compositionMask, const Camera& camera )
{
    m_scrollFactor += frameTime * 1.0f * speed;
    m_rotationFactor += frameTime * 2.0f * speed;
    m_flipTimer += frameTime * 1.0f;

    if ( m_scrollFactor > 10.0f )
        m_scrollFactor -= 10.0f;

    const float twoPI = 6.283185307179586476925286766559f;

    if ( m_rotationFactor > twoPI )
        m_rotationFactor -= twoPI;

    int textureIndex = min( (int)floorf( m_flipTimer * 0.33333f ), _countof( m_textures ) - 1 );
    if ( m_flipTimer > 9.0f )
        m_flipTimer = 0.0f;

    VkDescriptorBufferInfo cb = {};
    ConstantBuffer* constantBuffer = nullptr;
    m_constantBufferRing->AllocConstantBuffer( sizeof(*constantBuffer), (void**)&constantBuffer, &cb );

    constantBuffer->currentViewProj = camera.GetProjection() * camera.GetView();
    constantBuffer->previousViewProj = camera.GetPrevProjection() * camera.GetPrevView();

    constantBuffer->jitterCompensation[0] = camera.GetPrevProjection().getCol2().getX() - camera.GetProjection().getCol2().getX();
    constantBuffer->jitterCompensation[1] = camera.GetPrevProjection().getCol2().getY() - camera.GetProjection().getCol2().getY();
    constantBuffer->scrollFactor = m_scrollFactor;
    constantBuffer->rotationFactor = m_rotationFactor;
    constantBuffer->mode = textureIndex;

    uint32_t uniformOffsets[] = { (uint32_t)cb.offset };
    vkCmdBindDescriptorSets( commandList, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSets[textureIndex], _countof( uniformOffsets ), uniformOffsets );
    vkCmdBindPipeline( commandList, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelines[compositionMask ? 0 : 1] );
    vkCmdBindIndexBuffer( commandList, m_indexBuffer.buffer, m_indexBuffer.offset, VK_INDEX_TYPE_UINT32 );
    vkCmdDrawIndexed( commandList, 6, 2, 0, 0, 0 );
}
