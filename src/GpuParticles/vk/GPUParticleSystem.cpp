//
// Copyright (c) 2019 Advanced Micro Devices, Inc. All rights reserved.
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
#include "../vk/stdafx.h"
#include "BufferHelper.h"
#include "../ParticleSystem.h"
#include "../ParticleHelpers.h"
#include "../ParticleSystemInternal.h"
#include "ParallelSort.h"
#include "base/ExtDebugUtils.h"


size_t CAULDRON_VK::FormatSize(VkFormat format)
{
    switch (format)
    {
    case VK_FORMAT_R8_SINT: return 1;//(BYTE)
    case VK_FORMAT_R8_UINT: return 1;//(UNSIGNED_BYTE)1
    case VK_FORMAT_R16_SINT: return 2;//(SHORT)2
    case VK_FORMAT_R16_UINT: return 2;//(UNSIGNED_SHORT)2
    case VK_FORMAT_R32_SINT: return 4;//(SIGNED_INT)4
    case VK_FORMAT_R32_UINT: return 4;//(UNSIGNED_INT)4
    case VK_FORMAT_R32_SFLOAT: return 4;//(FLOAT)

    case VK_FORMAT_R8G8_SINT: return 2 * 1;//(BYTE)
    case VK_FORMAT_R8G8_UINT: return 2 * 1;//(UNSIGNED_BYTE)1
    case VK_FORMAT_R16G16_SINT: return 2 * 2;//(SHORT)2
    case VK_FORMAT_R16G16_UINT: return 2 * 2; // (UNSIGNED_SHORT)2
    case VK_FORMAT_R32G32_SINT: return 2 * 4;//(SIGNED_INT)4
    case VK_FORMAT_R32G32_UINT: return 2 * 4;//(UNSIGNED_INT)4
    case VK_FORMAT_R32G32_SFLOAT: return 2 * 4;//(FLOAT)

    case VK_FORMAT_UNDEFINED: return 0;//(BYTE) (UNSIGNED_BYTE) (SHORT) (UNSIGNED_SHORT)
    case VK_FORMAT_R32G32B32_SINT: return 3 * 4;//(SIGNED_INT)4
    case VK_FORMAT_R32G32B32_UINT: return 3 * 4;//(UNSIGNED_INT)4
    case VK_FORMAT_R32G32B32_SFLOAT: return 3 * 4;//(FLOAT)

    case VK_FORMAT_R8G8B8A8_SINT: return 4 * 1;//(BYTE)
    case VK_FORMAT_R8G8B8A8_UINT: return 4 * 1;//(UNSIGNED_BYTE)1
    case VK_FORMAT_R16G16B16A16_SINT: return 4 * 2;//(SHORT)2
    case VK_FORMAT_R16G16B16A16_UINT: return 4 * 2;//(UNSIGNED_SHORT)2
    case VK_FORMAT_R32G32B32A32_SINT: return 4 * 4;//(SIGNED_INT)4
    case VK_FORMAT_R32G32B32A32_UINT: return 4 * 4;//(UNSIGNED_INT)4
    case VK_FORMAT_R32G32B32A32_SFLOAT: return 4 * 4;//(FLOAT)

    case VK_FORMAT_R16G16B16A16_SFLOAT: return 4 * 2;
    }

    assert(0);
    return 0;
}

#pragma warning( disable : 4100 ) // disable unreference formal parameter warnings for /W4 builds

struct IndirectCommand
{
    int args[ 5 ];
};

// GPU Particle System class. Responsible for updating and rendering the particles
class GPUParticleSystem : public IParticleSystem
{
public:

    GPUParticleSystem( const char* particleAtlas );

private:

    enum DepthCullingMode
    {
        DepthCullingOn,
        DepthCullingOff,
        NumDepthCullingModes
    };

    enum StreakMode
    {
        StreaksOn,
        StreaksOff,
        NumStreakModes
    };

    enum ReactiveMode
    {
        ReactiveOn,
        ReactiveOff,
        NumReactiveModes
    };

    virtual ~GPUParticleSystem();

    virtual void OnCreateDevice( Device& device, UploadHeap& uploadHeap, ResourceViewHeaps& heaps, StaticBufferPool& bufferPool, DynamicBufferRing& constantBufferRing, VkRenderPass renderPass );
    virtual void OnResizedSwapChain( int width, int height, Texture& depthBuffer, VkFramebuffer frameBuffer );
    virtual void OnReleasingSwapChain();
    virtual void OnDestroyDevice();

    virtual void Reset();

    virtual void Render( VkCommandBuffer commandBuffer, DynamicBufferRing& constantBufferRing, int flags, const EmitterParams* pEmitters, int nNumEmitters, const ConstantData& constantData );

    void Emit( VkCommandBuffer commandBuffer, DynamicBufferRing& constantBufferRing, uint32_t perFrameConstantOffset, int numEmitters, const EmitterParams* emitters );
    void Simulate( VkCommandBuffer commandBuffer );
    void Sort( VkCommandBuffer commandBuffer );

    void FillRandomTexture( UploadHeap& uploadHeap );
    void CreateSimulationAssets( DynamicBufferRing& constantBufferRing );
    void CreateRasterizedRenderingAssets( DynamicBufferRing& constantBufferRing );

    VkPipeline CreatePipeline( const char* filename, const char* entry, VkPipelineLayout layout, const DefineList* defines );

    Device*                     m_pDevice = nullptr;
    ResourceViewHeaps*          m_heaps = nullptr;
    const char*                 m_AtlasPath = nullptr;
    VkRenderPass                m_renderPass = VK_NULL_HANDLE;
    VkFramebuffer               m_frameBuffer = VK_NULL_HANDLE;

    Texture                     m_Atlas = {};
    VkImageView                 m_AtlasSRV = {};
    Buffer                      m_ParticleBufferA = {};
    Buffer                      m_ParticleBufferB = {};
    Buffer                      m_PackedViewSpaceParticlePositions = {};
    Buffer                      m_MaxRadiusBuffer = {};
    Buffer                      m_DeadListBuffer = {};
    Buffer                      m_AliveCountBuffer = {};
    Buffer                      m_AliveIndexBuffer = {};
    Buffer                      m_AliveDistanceBuffer = {};
    Buffer                      m_DstAliveIndexBuffer = {};         // working memory for the Radix sorter
    Buffer                      m_DstAliveDistanceBuffer = {};      // working memory for the Radix sorter
    Buffer                      m_IndirectArgsBuffer = {};

    Texture                     m_RandomTexture = {};
    VkImageView                 m_RandomTextureSRV = {};

    VkImage                     m_DepthBuffer = {};
    VkImageView                 m_DepthBufferSRV = {};

    VkDescriptorSetLayout       m_SimulationDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet             m_SimulationDescriptorSet = VK_NULL_HANDLE;

    VkDescriptorSetLayout       m_RasterizationDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet             m_RasterizationDescriptorSet = VK_NULL_HANDLE;

    VkSampler                   m_samplers[ 3 ] = {};

    UINT                        m_ScreenWidth = 0;
    UINT                        m_ScreenHeight = 0;
    float                       m_InvScreenWidth = 0.0f;
    float                       m_InvScreenHeight = 0.0f;
    float                       m_ElapsedTime = 0.0f;
    float                       m_AlphaThreshold = 0.97f;

    VkDescriptorBufferInfo      m_IndexBuffer = {};

    VkPipelineLayout            m_SimulationPipelineLayout = VK_NULL_HANDLE;
    VkPipelineLayout            m_RasterizationPipelineLayout = VK_NULL_HANDLE;

    VkPipeline                  m_SimulationPipeline = VK_NULL_HANDLE;
    VkPipeline                  m_EmitPipeline = VK_NULL_HANDLE;
    VkPipeline                  m_ResetParticlesPipeline = VK_NULL_HANDLE;
    VkPipeline                  m_RasterizationPipelines[ NumStreakModes ][ NumReactiveModes ] = {};

    bool                        m_ResetSystem = true;
    FFXParallelSort             m_SortLib = {};
};


IParticleSystem* IParticleSystem::CreateGPUSystem( const char* particleAtlas )
{
    return new GPUParticleSystem(  particleAtlas );
}


GPUParticleSystem::GPUParticleSystem( const char* particleAtlas ) : m_AtlasPath( particleAtlas ) {}
GPUParticleSystem::~GPUParticleSystem() {}


// Use the sort lib to perform a bitonic sort over the particle indices based on their distance from camera
void GPUParticleSystem::Sort( VkCommandBuffer commandBuffer )
{
    m_SortLib.Draw( commandBuffer );
}


void GPUParticleSystem::Reset()
{
    m_ResetSystem = true;
}


void GPUParticleSystem::Render( VkCommandBuffer commandBuffer, DynamicBufferRing& constantBufferRing, int flags, const EmitterParams* pEmitters, int nNumEmitters, const ConstantData& constantData )
{
    SimulationConstantBuffer simulationConstants = {};

    memcpy( simulationConstants.m_StartColor, constantData.m_StartColor, sizeof( simulationConstants.m_StartColor ) );
    memcpy( simulationConstants.m_EndColor, constantData.m_EndColor, sizeof( simulationConstants.m_EndColor ) );
    memcpy( simulationConstants.m_EmitterLightingCenter, constantData.m_EmitterLightingCenter, sizeof( simulationConstants.m_EmitterLightingCenter ) );

    simulationConstants.m_ViewProjection = constantData.m_ViewProjection;
    simulationConstants.m_View = constantData.m_View;
    simulationConstants.m_ViewInv = constantData.m_ViewInv;
    simulationConstants.m_ProjectionInv = constantData.m_ProjectionInv;

    simulationConstants.m_EyePosition = constantData.m_ViewInv.getCol3();
    simulationConstants.m_SunDirection = constantData.m_SunDirection;

    simulationConstants.m_ScreenWidth = m_ScreenWidth;
    simulationConstants.m_ScreenHeight = m_ScreenHeight;
    simulationConstants.m_MaxParticles = g_maxParticles;
    simulationConstants.m_FrameTime = constantData.m_FrameTime;

    math::Vector4 sunDirectionVS = constantData.m_View * constantData.m_SunDirection;

    m_ElapsedTime += constantData.m_FrameTime;
    if ( m_ElapsedTime > 10.0f )
        m_ElapsedTime -= 10.0f;

    simulationConstants.m_ElapsedTime = m_ElapsedTime;

    void* data = nullptr;
    VkDescriptorBufferInfo constantBuffer = {};
    constantBufferRing.AllocConstantBuffer( sizeof( simulationConstants ), &data, &constantBuffer );
    memcpy( data, &simulationConstants, sizeof( simulationConstants ) );

    {
        uint32_t uniformOffsets[] = { (uint32_t)constantBuffer.offset, 0 };
        vkCmdBindDescriptorSets( commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_SimulationPipelineLayout, 0, 1, &m_SimulationDescriptorSet, _countof( uniformOffsets ), uniformOffsets );


        UserMarker marker( commandBuffer, "simulation" );

        // If we are resetting the particle system, then initialize the dead list
        if ( m_ResetSystem )
        {
            vkCmdBindPipeline( commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_ResetParticlesPipeline );

            // Disaptch a set of 1d thread groups to fill out the dead list, one thread per particle
            vkCmdDispatch( commandBuffer, align( g_maxParticles, 256 ) / 256, 1, 1 );

            std::vector<VkBufferMemoryBarrier> barriers = {};
            m_ParticleBufferA.AddPipelineBarrier( barriers, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
            m_ParticleBufferB.AddPipelineBarrier( barriers, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
            m_DeadListBuffer.AddPipelineBarrier( barriers, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
            vkCmdPipelineBarrier( commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, (uint32_t)barriers.size(), &barriers[ 0 ], 0, nullptr );

            m_ResetSystem = false;
        }

        // Emit particles into the system
        Emit( commandBuffer, constantBufferRing, (uint32_t)constantBuffer.offset, nNumEmitters, pEmitters );

        // Run the simulation for this frame
        Simulate( commandBuffer );

        std::vector<VkBufferMemoryBarrier> barriersAfterSimulation = {};
        m_ParticleBufferA.AddPipelineBarrier( barriersAfterSimulation, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
        m_PackedViewSpaceParticlePositions.AddPipelineBarrier( barriersAfterSimulation, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
        m_MaxRadiusBuffer.AddPipelineBarrier( barriersAfterSimulation, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
        m_DeadListBuffer.AddPipelineBarrier( barriersAfterSimulation, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
        m_AliveCountBuffer.AddPipelineBarrier( barriersAfterSimulation, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.image = m_DepthBuffer;

        vkCmdPipelineBarrier( commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, (uint32_t)barriersAfterSimulation.size(), &barriersAfterSimulation[ 0 ], 1, &barrier );
    }

    {
        UserMarker marker( commandBuffer, "rasterization" );

        // Sort if requested. Not doing so results in the particles rendering out of order and not blending correctly
        if ( flags & PF_Sort )
        {
            UserMarker marker( commandBuffer, "sorting" );

            std::vector<VkBufferMemoryBarrier> barriers = {};
            m_AliveIndexBuffer.AddPipelineBarrier( barriers, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
            m_AliveDistanceBuffer.AddPipelineBarrier( barriers, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
            vkCmdPipelineBarrier( commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, (uint32_t)barriers.size(), &barriers[ 0 ], 0, nullptr );

            Sort( commandBuffer );
        }

        std::vector<VkBufferMemoryBarrier> barriers = {};
        m_AliveIndexBuffer.AddPipelineBarrier( barriers, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT );
        m_IndirectArgsBuffer.AddPipelineBarrier( barriers, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT );
        vkCmdPipelineBarrier( commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, (uint32_t)barriers.size(), &barriers[ 0 ], 0, nullptr );

        RenderingConstantBuffer* cb = nullptr;
        VkDescriptorBufferInfo constantBuffer = {};
        constantBufferRing.AllocConstantBuffer( sizeof( RenderingConstantBuffer ), (void**)&cb, &constantBuffer );
        cb->m_Projection = constantData.m_Projection;
        cb->m_ProjectionInv = simulationConstants.m_ProjectionInv;
        cb->m_SunColor = constantData.m_SunColor;
        cb->m_AmbientColor = constantData.m_AmbientColor;
        cb->m_SunDirectionVS = sunDirectionVS;
        cb->m_ScreenWidth = m_ScreenWidth;
        cb->m_ScreenHeight = m_ScreenHeight;

        uint32_t uniformOffsets[1] = { (uint32_t)constantBuffer.offset };
        vkCmdBindDescriptorSets( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_RasterizationPipelineLayout, 0, 1, &m_RasterizationDescriptorSet, 1, uniformOffsets );

        VkRenderPassBeginInfo renderPassBegin = {};
        renderPassBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBegin.renderPass = m_renderPass;
        renderPassBegin.framebuffer = m_frameBuffer;
        renderPassBegin.renderArea.extent.width = m_ScreenWidth;
        renderPassBegin.renderArea.extent.height = m_ScreenHeight;

        vkCmdBeginRenderPass( commandBuffer, &renderPassBegin, VK_SUBPASS_CONTENTS_INLINE );

        StreakMode streaks = flags & PF_Streaks ? StreaksOn : StreaksOff;
        ReactiveMode reactive = flags & PF_Reactive ? ReactiveOn : ReactiveOff;

        vkCmdBindPipeline( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_RasterizationPipelines[ streaks ][ reactive ] );

        vkCmdBindIndexBuffer( commandBuffer, m_IndexBuffer.buffer, m_IndexBuffer.offset, VK_INDEX_TYPE_UINT32 );

        vkCmdDrawIndexedIndirect( commandBuffer, m_IndirectArgsBuffer.Resource(), 0, 1, sizeof( IndirectCommand ) );

        vkCmdEndRenderPass( commandBuffer );
    }
}


void GPUParticleSystem::OnCreateDevice( Device& device, UploadHeap& uploadHeap, ResourceViewHeaps& heaps, StaticBufferPool& bufferPool, DynamicBufferRing& constantBufferRing, VkRenderPass renderPass )
{
    m_pDevice = &device;
    m_heaps = &heaps;
    m_renderPass = renderPass;

    VkSamplerCreateInfo sampler = {};
    sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler.minLod = 0.0f;
    sampler.maxLod =  FLT_MAX;
    sampler.mipLodBias = 0.0f;
    sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    sampler.compareEnable = VK_FALSE;
    sampler.compareOp = VK_COMPARE_OP_NEVER;
    sampler.maxAnisotropy = 1.0f;
    sampler.anisotropyEnable = VK_FALSE;

    for ( int i = 0; i < 3; i++ )
    {
        if ( i == 1 )
        {
            sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        }
        else
        {
            sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        }

        if ( i == 2 )
        {
            sampler.magFilter = VK_FILTER_NEAREST;
            sampler.minFilter = VK_FILTER_NEAREST;
            sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        }
        else
        {
            sampler.magFilter = VK_FILTER_LINEAR;
            sampler.minFilter = VK_FILTER_LINEAR;
            sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        }

        vkCreateSampler( m_pDevice->GetDevice(), &sampler, nullptr, &m_samplers[ i ] );
    }

    // Create the global particle pool. Each particle is split into two parts for better cache coherency. The first half contains the data more 
    // relevant to rendering while the second half is more related to simulation
    m_ParticleBufferA.Init( m_pDevice, g_maxParticles, sizeof( GPUParticlePartA ), "ParticleBufferA", false );
    m_ParticleBufferB.Init( m_pDevice, g_maxParticles, sizeof( GPUParticlePartB ), "ParticleBufferB", false );

    // The packed view space positions of particles are cached during simulation so allocate a buffer for them
    m_PackedViewSpaceParticlePositions.Init( m_pDevice, g_maxParticles, sizeof( UINT ) * 2, "PackedViewSpaceParticlePositions", false );

    // The maximum radii of each particle is cached during simulation to avoid recomputing multiple times later. This is only required
    // for streaked particles as they are not round so we cache the max radius of X and Y
    m_MaxRadiusBuffer.Init( m_pDevice, g_maxParticles, 4, "MaxRadiusBuffer", false );

    // The dead particle index list. Created as an append buffer
    m_DeadListBuffer.Init( m_pDevice, g_maxParticles + 1, 4, "DeadListBuffer", false );

    // Create the buffer to hold the number of alive particles
    m_AliveCountBuffer.Init( m_pDevice, 1, 4, "AliveCountBuffer", false );

    // Create the index buffer of alive particles that is to be sorted (at least in the rasterization path).
    m_AliveIndexBuffer.Init( m_pDevice, g_maxParticles, 4, "AliveIndexBuffer", false );
    m_DstAliveIndexBuffer.Init( m_pDevice, g_maxParticles, 4, "DstAliveIndexBuffer", false );

    // Create the list of distances of each alive particle - used for sorting in the rasterization path.
    m_AliveDistanceBuffer.Init( m_pDevice, g_maxParticles, 4, "AliveDistanceBuffer", false );
    m_DstAliveDistanceBuffer.Init( m_pDevice, g_maxParticles, 4, "DstAliveDistanceBuffer", false );

    // Create the buffer to store the indirect args for the ExecuteIndirect call
    // Create the index buffer of alive particles that is to be sorted (at least in the rasterization path).
    m_IndirectArgsBuffer.Init( m_pDevice, 1, sizeof( IndirectCommand ), "IndirectArgsBuffer", true );

    // Create the particle billboard index buffer required for the rasterization VS-only path
    UINT* indices = new UINT[ g_maxParticles * 6 ];
    UINT* ptr = indices;
    UINT base = 0;
    for ( int i = 0; i < g_maxParticles; i++ )
    {
        ptr[ 0 ] = base + 0;
        ptr[ 1 ] = base + 1;
        ptr[ 2 ] = base + 2;

        ptr[ 3 ] = base + 2;
        ptr[ 4 ] = base + 1;
        ptr[ 5 ] = base + 3;

        base += 4;
        ptr += 6;
    }

    bufferPool.AllocBuffer( g_maxParticles * 6, sizeof( UINT ), indices, &m_IndexBuffer );
    delete[] indices;

    // Initialize the random numbers texture
    FillRandomTexture( uploadHeap );

    m_Atlas.InitFromFile( &device, &uploadHeap, m_AtlasPath, true );
    m_Atlas.CreateSRV( &m_AtlasSRV );

    CreateSimulationAssets( constantBufferRing );
    CreateRasterizedRenderingAssets( constantBufferRing );

    // Create the SortLib resources
    m_SortLib.OnCreate( &device, &heaps, &constantBufferRing, &uploadHeap, &m_AliveCountBuffer, &m_AliveDistanceBuffer, &m_AliveIndexBuffer, &m_DstAliveDistanceBuffer, &m_DstAliveIndexBuffer );
}


VkPipeline GPUParticleSystem::CreatePipeline( const char* filename, const char* entry, VkPipelineLayout layout, const DefineList* defines )
{
    VkPipelineShaderStageCreateInfo computeShader = {};
    VkResult res = VKCompileFromFile( m_pDevice->GetDevice(), VK_SHADER_STAGE_COMPUTE_BIT, filename, entry, "-T cs_6_0", defines, &computeShader );
    assert(res == VK_SUCCESS);

    VkComputePipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = layout;
    pipelineInfo.stage = computeShader;

    VkPipeline pipeline = {}; 
    res = vkCreateComputePipelines( m_pDevice->GetDevice(), m_pDevice->GetPipelineCache(), 1, &pipelineInfo, nullptr, &pipeline );
    assert(res == VK_SUCCESS);
    SetResourceName( m_pDevice->GetDevice(), VK_OBJECT_TYPE_PIPELINE, (uint64_t)pipeline, entry );

    return pipeline;
}


void GPUParticleSystem::CreateSimulationAssets( DynamicBufferRing& constantBufferRing )
{
    //  0 - g_ParticleBufferA
    //  1 - g_ParticleBufferB
    //  2 - g_DeadList
    //  3 - g_IndexBuffer
    //  4 - g_DistanceBuffer
    //  5 - g_MaxRadiusBuffer
    //  6 - g_PackedViewSpacePositions
    //  7 - g_DrawArgs
    //  8 - g_AliveParticleCount
    //  9 - g_DepthBuffer
    // 10 - g_RandomBuffer
    // 11 - PerFrameConstantBuffer
    // 12 - EmitterConstantBuffer
    // 13 - g_samWrapPoint

    std::vector<VkDescriptorSetLayoutBinding> layout_bindings( 14 );
    int binding = 0;
    for ( int i = 0; i < 9; i++ )
    {
        layout_bindings[binding].binding = binding;
        layout_bindings[binding].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        layout_bindings[binding].descriptorCount = 1;
        layout_bindings[binding].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        layout_bindings[binding].pImmutableSamplers = nullptr;
        binding++;
    }

    for ( int i = 0; i < 2; i++ )
    {
        layout_bindings[binding].binding = binding;
        layout_bindings[binding].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        layout_bindings[binding].descriptorCount = 1;
        layout_bindings[binding].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        layout_bindings[binding].pImmutableSamplers = nullptr;
        binding++;
    }
    for ( int i = 0; i < 2; i++ )
    {
        layout_bindings[binding].binding = binding;
        layout_bindings[binding].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        layout_bindings[binding].descriptorCount = 1;
        layout_bindings[binding].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        layout_bindings[binding].pImmutableSamplers = nullptr;
        binding++;
    }

    {
        layout_bindings[binding].binding = binding;
        layout_bindings[binding].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        layout_bindings[binding].descriptorCount = 1;
        layout_bindings[binding].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        layout_bindings[binding].pImmutableSamplers = &m_samplers[ 2 ];
        binding++;
    }

    assert( binding == layout_bindings.size() );

    m_heaps->CreateDescriptorSetLayoutAndAllocDescriptorSet( &layout_bindings, &m_SimulationDescriptorSetLayout, &m_SimulationDescriptorSet );
    constantBufferRing.SetDescriptorSet( 11, sizeof( SimulationConstantBuffer ), m_SimulationDescriptorSet );
    constantBufferRing.SetDescriptorSet( 12, sizeof( EmitterConstantBuffer ), m_SimulationDescriptorSet );

    // Create pipeline layout
    //

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &m_SimulationDescriptorSetLayout;

    VkResult res = vkCreatePipelineLayout( m_pDevice->GetDevice(), &pipelineLayoutCreateInfo, nullptr, &m_SimulationPipelineLayout );
    assert(res == VK_SUCCESS);

    m_ParticleBufferA.SetDescriptorSet( 0, m_SimulationDescriptorSet, true );
    m_ParticleBufferB.SetDescriptorSet( 1, m_SimulationDescriptorSet, true );
    m_DeadListBuffer.SetDescriptorSet( 2, m_SimulationDescriptorSet, true );
    m_AliveIndexBuffer.SetDescriptorSet( 3, m_SimulationDescriptorSet, true );
    m_AliveDistanceBuffer.SetDescriptorSet( 4, m_SimulationDescriptorSet, true );
    m_MaxRadiusBuffer.SetDescriptorSet( 5, m_SimulationDescriptorSet, true );
    m_PackedViewSpaceParticlePositions.SetDescriptorSet( 6, m_SimulationDescriptorSet, true );
    m_IndirectArgsBuffer.SetDescriptorSet( 7, m_SimulationDescriptorSet, true );
    m_AliveCountBuffer.SetDescriptorSet( 8, m_SimulationDescriptorSet, true );
    // depth buffer
    SetDescriptorSet( m_pDevice->GetDevice(), 10, m_RandomTextureSRV, nullptr, m_SimulationDescriptorSet );

    // Create pipelines
    //

    DefineList defines = {};
    defines[ "API_VULKAN" ] = "";

    m_ResetParticlesPipeline = CreatePipeline( "ParticleSimulation.hlsl", "CS_Reset", m_SimulationPipelineLayout, &defines );
    m_SimulationPipeline = CreatePipeline( "ParticleSimulation.hlsl", "CS_Simulate", m_SimulationPipelineLayout, &defines );
    m_EmitPipeline = CreatePipeline( "ParticleEmit.hlsl", "CS_Emit", m_SimulationPipelineLayout, &defines );
}


void GPUParticleSystem::CreateRasterizedRenderingAssets( DynamicBufferRing& constantBufferRing )
{
    //  0 - g_ParticleBufferA
    //  1 - g_PackedViewSpacePositions
    //  2 - g_NumParticlesBuffer
    //  3 - g_SortedIndexBuffer
    //  4 - g_ParticleTexture
    //  5 - g_DepthTexture
    //  6 - RenderingConstantBuffer
    //  7 - g_samClampLinear

    std::vector<VkDescriptorSetLayoutBinding> layout_bindings( 8 );
    for ( uint32_t i = 0; i < layout_bindings.size(); i++ )
    {
        layout_bindings[i].binding = i;
        layout_bindings[i].descriptorCount = 1;
        layout_bindings[i].pImmutableSamplers = nullptr;
    }

    layout_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layout_bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    layout_bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layout_bindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    layout_bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layout_bindings[2].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    layout_bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layout_bindings[3].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    layout_bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    layout_bindings[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    layout_bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    layout_bindings[5].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    layout_bindings[6].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    layout_bindings[6].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;

    layout_bindings[7].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    layout_bindings[7].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layout_bindings[7].pImmutableSamplers = &m_samplers[ 1 ];

    m_heaps->CreateDescriptorSetLayoutAndAllocDescriptorSet( &layout_bindings, &m_RasterizationDescriptorSetLayout, &m_RasterizationDescriptorSet );
    m_ParticleBufferA.SetDescriptorSet( 0, m_RasterizationDescriptorSet, false );
    m_PackedViewSpaceParticlePositions.SetDescriptorSet( 1, m_RasterizationDescriptorSet, false );
    m_AliveCountBuffer.SetDescriptorSet( 2, m_RasterizationDescriptorSet, false );
    m_AliveIndexBuffer.SetDescriptorSet( 3, m_RasterizationDescriptorSet, false );
    SetDescriptorSet( m_pDevice->GetDevice(), 4, m_AtlasSRV, nullptr, m_RasterizationDescriptorSet );
    // depth buffer
    constantBufferRing.SetDescriptorSet( 6, sizeof( RenderingConstantBuffer ), m_RasterizationDescriptorSet );

    // Create pipeline layout
    //

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &m_RasterizationDescriptorSetLayout;

    VkResult res = vkCreatePipelineLayout( m_pDevice->GetDevice(), &pipelineLayoutCreateInfo, nullptr, &m_RasterizationPipelineLayout );
    assert(res == VK_SUCCESS);

    // input assembly state and layout
    VkPipelineVertexInputStateCreateInfo vi = {};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo ia;
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.pNext = NULL;
    ia.flags = 0;
    ia.primitiveRestartEnable = VK_FALSE;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // rasterizer state
    VkPipelineRasterizationStateCreateInfo rs;
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.pNext = NULL;
    rs.flags = 0;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.depthClampEnable = VK_FALSE;
    rs.rasterizerDiscardEnable = VK_FALSE;
    rs.depthBiasEnable = VK_FALSE;
    rs.depthBiasConstantFactor = 0;
    rs.depthBiasClamp = 0;
    rs.depthBiasSlopeFactor = 0;
    rs.lineWidth = 1.0f;

    VkPipelineColorBlendAttachmentState att_state[4] = {};
    att_state[0].colorWriteMask = 0xf;
    att_state[0].blendEnable = VK_TRUE;
    att_state[0].alphaBlendOp = VK_BLEND_OP_ADD;
    att_state[0].colorBlendOp = VK_BLEND_OP_ADD;
    att_state[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    att_state[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    att_state[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    att_state[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    att_state[1].colorWriteMask = 0x0;
    att_state[2].colorWriteMask = 0xf;
    att_state[3].colorWriteMask = 0x0;
    
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

    VkDynamicState dynamicStateEnables[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.pNext = NULL;
    dynamicState.pDynamicStates = dynamicStateEnables;
    dynamicState.dynamicStateCount = _countof( dynamicStateEnables );

    // view port state
    VkPipelineViewportStateCreateInfo vp = {};
    vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.scissorCount = 1;

    // depth stencil state
    VkPipelineDepthStencilStateCreateInfo ds = {};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = VK_TRUE;
    ds.depthWriteEnable = VK_FALSE;
    ds.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
    ds.depthBoundsTestEnable = VK_FALSE;
    ds.stencilTestEnable = VK_FALSE;
    ds.back.failOp = VK_STENCIL_OP_KEEP;
    ds.back.passOp = VK_STENCIL_OP_KEEP;
    ds.back.compareOp = VK_COMPARE_OP_ALWAYS;
    ds.back.compareMask = 0;
    ds.back.reference = 0;
    ds.back.depthFailOp = VK_STENCIL_OP_KEEP;
    ds.back.writeMask = 0;
    ds.minDepthBounds = 0;
    ds.maxDepthBounds = 0;
    ds.stencilTestEnable = VK_FALSE;
    ds.front = ds.back;

    // multi sample state
    VkPipelineMultisampleStateCreateInfo ms = {};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    for ( int i = 0; i < NumStreakModes; i++ )
    {
        for ( int j = 0; j < NumReactiveModes; j++ )
        {
            att_state[2].colorWriteMask = 0x0;

            DefineList defines;
            if ( i == StreaksOn )
                defines[ "STREAKS" ] = "";

            if ( j == ReactiveOn )
            {
                defines["REACTIVE"] = "";
                att_state[2].colorWriteMask = 0xf;
            }

            // Compile shaders
            //
            VkPipelineShaderStageCreateInfo vertexShader = {};
            res = VKCompileFromFile(m_pDevice->GetDevice(), VK_SHADER_STAGE_VERTEX_BIT, "ParticleRender.hlsl", "VS_StructuredBuffer", "-T vs_6_0", &defines, &vertexShader );
            assert(res == VK_SUCCESS);

            VkPipelineShaderStageCreateInfo fragmentShader;
            res = VKCompileFromFile(m_pDevice->GetDevice(), VK_SHADER_STAGE_FRAGMENT_BIT, "ParticleRender.hlsl", "PS_Billboard", "-T ps_6_0", &defines, &fragmentShader );
            assert(res == VK_SUCCESS);

            VkPipelineShaderStageCreateInfo shaderStages[] = { vertexShader, fragmentShader };

            // Create pipeline
            //
            VkGraphicsPipelineCreateInfo pipeline = {};
            pipeline.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            pipeline.layout = m_RasterizationPipelineLayout;
            pipeline.pVertexInputState = &vi;
            pipeline.pInputAssemblyState = &ia;
            pipeline.pRasterizationState = &rs;
            pipeline.pMultisampleState = &ms;
            pipeline.pColorBlendState = &cb;
            pipeline.pDynamicState = &dynamicState;
            pipeline.pViewportState = &vp;
            pipeline.pDepthStencilState = &ds;
            pipeline.pStages = shaderStages;
            pipeline.stageCount = _countof( shaderStages );
            pipeline.renderPass = m_renderPass;

            res = vkCreateGraphicsPipelines( m_pDevice->GetDevice(), m_pDevice->GetPipelineCache(), 1, &pipeline, nullptr, &m_RasterizationPipelines[ i ][ j ] );
            assert(res == VK_SUCCESS);
        }
    }
}


void GPUParticleSystem::OnResizedSwapChain( int width, int height, Texture& depthBuffer, VkFramebuffer frameBuffer )
{
    m_frameBuffer = frameBuffer;
    m_ScreenWidth = width;
    m_ScreenHeight = height;
    m_InvScreenWidth = 1.0f / m_ScreenWidth;
    m_InvScreenHeight = 1.0f / m_ScreenHeight;

    m_DepthBuffer = depthBuffer.Resource();
    depthBuffer.CreateSRV( &m_DepthBufferSRV );

    SetDescriptorSetForDepth( m_pDevice->GetDevice(), 9, m_DepthBufferSRV, nullptr, m_SimulationDescriptorSet );
    SetDescriptorSetForDepth( m_pDevice->GetDevice(), 5, m_DepthBufferSRV, nullptr, m_RasterizationDescriptorSet );
}


void GPUParticleSystem::OnReleasingSwapChain()
{ 
    if (m_DepthBufferSRV != nullptr)
    {
        vkDestroyImageView(m_pDevice->GetDevice(), m_DepthBufferSRV, nullptr);
        m_DepthBufferSRV = {};
    }
}


void GPUParticleSystem::OnDestroyDevice()
{
    m_ParticleBufferA.OnDestroy();
    m_ParticleBufferB.OnDestroy();
    m_PackedViewSpaceParticlePositions.OnDestroy();
    m_MaxRadiusBuffer.OnDestroy();
    m_DeadListBuffer.OnDestroy();
    m_AliveDistanceBuffer.OnDestroy();
    m_AliveIndexBuffer.OnDestroy();
    m_DstAliveDistanceBuffer.OnDestroy();
    m_DstAliveIndexBuffer.OnDestroy();
    m_AliveCountBuffer.OnDestroy();
    vkDestroyImageView( m_pDevice->GetDevice(), m_RandomTextureSRV, nullptr );
    m_RandomTexture.OnDestroy();
    vkDestroyImageView( m_pDevice->GetDevice(), m_AtlasSRV, nullptr );
    m_Atlas.OnDestroy();
    m_IndirectArgsBuffer.OnDestroy();

    vkDestroyDescriptorSetLayout( m_pDevice->GetDevice(), m_SimulationDescriptorSetLayout, nullptr );
    vkDestroyDescriptorSetLayout( m_pDevice->GetDevice(), m_RasterizationDescriptorSetLayout, nullptr );

    vkDestroyPipeline( m_pDevice->GetDevice(), m_SimulationPipeline, nullptr );
    vkDestroyPipeline( m_pDevice->GetDevice(), m_ResetParticlesPipeline, nullptr );
    vkDestroyPipeline( m_pDevice->GetDevice(), m_EmitPipeline, nullptr );

    for ( int i = 0; i < NumStreakModes; i++ )
    {
        for ( int j = 0; j < NumReactiveModes; j++ )
        {
            vkDestroyPipeline( m_pDevice->GetDevice(), m_RasterizationPipelines[ i ][ j ], nullptr );
        }
    }

    vkDestroyPipelineLayout( m_pDevice->GetDevice(), m_SimulationPipelineLayout, nullptr );
    vkDestroyPipelineLayout( m_pDevice->GetDevice(), m_RasterizationPipelineLayout, nullptr );

    m_SortLib.OnDestroy();

    for ( int i = 0; i < _countof( m_samplers ); i++ )
    {
        vkDestroySampler( m_pDevice->GetDevice(), m_samplers[ i ], nullptr );
    }

    m_ResetSystem = true;
    m_pDevice = nullptr;
}


// Per-frame emission of particles into the GPU simulation
void GPUParticleSystem::Emit( VkCommandBuffer commandBuffer, DynamicBufferRing& constantBufferRing, uint32_t perFrameConstantOffset, int numEmitters, const EmitterParams* emitters )
{
    vkCmdBindPipeline( commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_EmitPipeline );

    // Run CS for each emitter
    for ( int i = 0; i < numEmitters; i++ )
    {
        const EmitterParams& emitter = emitters[ i ];

        if ( emitter.m_NumToEmit > 0 )
        {
            EmitterConstantBuffer* constants = nullptr;
            VkDescriptorBufferInfo constantBuffer = {};
            constantBufferRing.AllocConstantBuffer( sizeof(*constants), (void**)&constants, &constantBuffer );
            constants->m_EmitterPosition = emitter.m_Position;
            constants->m_EmitterVelocity = emitter.m_Velocity;
            constants->m_MaxParticlesThisFrame = emitter.m_NumToEmit;
            constants->m_ParticleLifeSpan = emitter.m_ParticleLifeSpan;
            constants->m_StartSize = emitter.m_StartSize;
            constants->m_EndSize = emitter.m_EndSize;
            constants->m_PositionVariance = emitter.m_PositionVariance;
            constants->m_VelocityVariance = emitter.m_VelocityVariance;
            constants->m_Mass = emitter.m_Mass;
            constants->m_Index = i;
            constants->m_Streaks = emitter.m_Streaks ? 1 : 0;
            constants->m_TextureIndex = emitter.m_TextureIndex;

            uint32_t uniformOffsets[] = { perFrameConstantOffset, (uint32_t)constantBuffer.offset };
            vkCmdBindDescriptorSets( commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_SimulationPipelineLayout, 0, 1, &m_SimulationDescriptorSet, _countof( uniformOffsets ), uniformOffsets );

            // Dispatch enough thread groups to spawn the requested particles
            int numThreadGroups = align( emitter.m_NumToEmit, 1024 ) / 1024;
            vkCmdDispatch( commandBuffer, numThreadGroups, 1, 1 );
        }
    }

    // RaW barriers
    m_ParticleBufferA.PipelineBarrier( commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
    m_ParticleBufferB.PipelineBarrier( commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
    m_DeadListBuffer.PipelineBarrier( commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
}


// Per-frame simulation step
void GPUParticleSystem::Simulate( VkCommandBuffer commandBuffer )
{
    vkCmdBindPipeline( commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_SimulationPipeline );
    vkCmdDispatch( commandBuffer, align( g_maxParticles, 256 ) / 256, 1, 1 );
}

// Populate a texture with random numbers (used for the emission of particles)
void GPUParticleSystem::FillRandomTexture( UploadHeap& uploadHeap )
{
    IMG_INFO header = {};
    header.width = 1024;
    header.height = 1024;
    header.depth = 1;
    header.arraySize = 1;
    header.mipMapCount = 1;
    header.format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    header.bitCount = 128;

    float* values = new float[ header.width * header.height * 4 ];
    float* ptr = values;
    for ( UINT i = 0; i < header.width * header.height; i++ )
    {
        ptr[ 0 ] = RandomVariance( 0.0f, 1.0f );
        ptr[ 1 ] = RandomVariance( 0.0f, 1.0f );
        ptr[ 2 ] = RandomVariance( 0.0f, 1.0f );
        ptr[ 3 ] = RandomVariance( 0.0f, 1.0f );
        ptr += 4;
    }

    m_RandomTexture.InitFromData( m_pDevice, uploadHeap, header, values, "RandomTexture" );
    m_RandomTexture.CreateSRV( &m_RandomTextureSRV );

    delete[] values;
}
