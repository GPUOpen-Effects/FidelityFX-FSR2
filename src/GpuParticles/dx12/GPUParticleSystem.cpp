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
#include "../DX12/stdafx.h"
#include "../ParticleSystem.h"
#include "../ParticleSystemInternal.h"
#include "../ParticleHelpers.h"
#include "ParallelSort.h"


const D3D12_RESOURCE_STATES SHADER_READ_STATE = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER|D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;


#pragma warning( disable : 4100 ) // disable unreference formal parameter warnings for /W4 builds

struct IndirectCommand
{
    D3D12_GPU_VIRTUAL_ADDRESS       uav = {};
    D3D12_DRAW_INDEXED_ARGUMENTS    drawArguments = {};
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

    virtual void OnCreateDevice( Device &device, UploadHeap& uploadHeap, ResourceViewHeaps& heaps, StaticBufferPool& bufferPool, DynamicBufferRing& constantBufferRing );
    virtual void OnResizedSwapChain( int width, int height, Texture& depthBuffer );
    virtual void OnReleasingSwapChain();
    virtual void OnDestroyDevice();

    virtual void Reset();

    virtual void Render( ID3D12GraphicsCommandList* pCommandList, DynamicBufferRing& constantBufferRing, int flags, const EmitterParams* pEmitters, int nNumEmitters, const ConstantData& constantData );

    void Emit( ID3D12GraphicsCommandList* pCommandList, DynamicBufferRing& constantBufferRing, int numEmitters, const EmitterParams* emitters );
    void Simulate( ID3D12GraphicsCommandList* pCommandList );
    void Sort( ID3D12GraphicsCommandList* pCommandList );

    void FillRandomTexture( UploadHeap& uploadHeap );

    void CreateSimulationAssets();
    void CreateRasterizedRenderingAssets();

    Device*                     m_pDevice = nullptr;
    ResourceViewHeaps*          m_heaps = nullptr;
    const char*                 m_AtlasPath = nullptr;

    Texture                     m_Atlas = {};
    Texture                     m_ParticleBufferA = {};
    Texture                     m_ParticleBufferB = {};
    Texture                     m_PackedViewSpaceParticlePositions = {};
    Texture                     m_MaxRadiusBuffer = {};
    Texture                     m_DeadListBuffer = {};
    Texture                     m_AliveIndexBuffer = {};
    Texture                     m_AliveDistanceBuffer = {};
    Texture                     m_AliveCountBuffer = {};
    Texture                     m_RenderingBuffer = {};
    Texture                     m_IndirectArgsBuffer = {};
    Texture                     m_RandomTexture = {};

    const int                   m_SimulationUAVDescriptorTableCount = 9;
    CBV_SRV_UAV                 m_SimulationUAVDescriptorTable = {};

    const int                   m_SimulationSRVDescriptorTableCount = 2;
    CBV_SRV_UAV                 m_SimulationSRVDescriptorTable = {};

    const int                   m_RasterizationSRVDescriptorTableCount = 6;
    CBV_SRV_UAV                 m_RasterizationSRVDescriptorTable = {};

    UINT                        m_ScreenWidth = 0;
    UINT                        m_ScreenHeight = 0;
    float                       m_InvScreenWidth = 0.0f;
    float                       m_InvScreenHeight = 0.0f;
    float                       m_ElapsedTime = 0.0f;
    float                       m_AlphaThreshold = 0.97f;

    D3D12_INDEX_BUFFER_VIEW     m_IndexBuffer = {};
    ID3D12RootSignature*        m_pSimulationRootSignature = nullptr;
    ID3D12RootSignature*        m_pRasterizationRootSignature = nullptr;

    ID3D12PipelineState*        m_pSimulatePipeline = nullptr;
    ID3D12PipelineState*        m_pEmitPipeline = nullptr;
    ID3D12PipelineState*        m_pResetParticlesPipeline = nullptr;
    ID3D12PipelineState*        m_pRasterizationPipelines[ NumStreakModes ][ NumReactiveModes ] = {};

    ID3D12CommandSignature*     m_commandSignature = nullptr;

    bool                        m_ResetSystem = true;
    FFXParallelSort             m_SortLib = {};

    D3D12_RESOURCE_STATES       m_ReadBufferStates;
    D3D12_RESOURCE_STATES       m_WriteBufferStates;
    D3D12_RESOURCE_STATES       m_StridedBufferStates;
};

IParticleSystem* IParticleSystem::CreateGPUSystem( const char* particleAtlas )
{
    return new GPUParticleSystem( particleAtlas );
}


GPUParticleSystem::GPUParticleSystem( const char* particleAtlas ) : m_AtlasPath( particleAtlas )
{
}


GPUParticleSystem::~GPUParticleSystem()
{
}


void GPUParticleSystem::Sort( ID3D12GraphicsCommandList* pCommandList )
{
    // Causes the debug layer to lock up
    m_SortLib.Draw( pCommandList );
}


void GPUParticleSystem::Reset()
{
    m_ResetSystem = true;
}

void GPUParticleSystem::Render( ID3D12GraphicsCommandList* pCommandList, DynamicBufferRing& constantBufferRing, int flags, const EmitterParams* pEmitters, int nNumEmitters, const ConstantData& constantData )
{
    std::vector<D3D12_RESOURCE_BARRIER> barriersBeforeSimulation;
    if(m_WriteBufferStates == D3D12_RESOURCE_STATE_COMMON)
    {
        barriersBeforeSimulation.push_back(CD3DX12_RESOURCE_BARRIER::Transition(m_ParticleBufferB.GetResource(), m_WriteBufferStates, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
        barriersBeforeSimulation.push_back(CD3DX12_RESOURCE_BARRIER::Transition(m_DeadListBuffer.GetResource(), m_WriteBufferStates, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
        barriersBeforeSimulation.push_back(CD3DX12_RESOURCE_BARRIER::Transition(m_AliveDistanceBuffer.GetResource(), m_WriteBufferStates, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
        barriersBeforeSimulation.push_back(CD3DX12_RESOURCE_BARRIER::Transition(m_IndirectArgsBuffer.GetResource(), m_WriteBufferStates, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
        m_WriteBufferStates = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }

    ID3D12DescriptorHeap* descriptorHeaps[] = { m_heaps->GetCBV_SRV_UAVHeap(), m_heaps->GetSamplerHeap() };
    pCommandList->SetDescriptorHeaps( _countof( descriptorHeaps ), descriptorHeaps );

    SimulationConstantBuffer simulationConstants = {};

    memcpy( simulationConstants.m_StartColor, constantData.m_StartColor, sizeof( simulationConstants.m_StartColor ) );
    memcpy( simulationConstants.m_EndColor, constantData.m_EndColor, sizeof( simulationConstants.m_EndColor ) );
    memcpy( simulationConstants.m_EmitterLightingCenter, constantData.m_EmitterLightingCenter, sizeof( simulationConstants.m_EmitterLightingCenter ) );

    simulationConstants.m_ViewProjection = constantData.m_ViewProjection;
    simulationConstants.m_View = constantData.m_View;
    simulationConstants.m_ViewInv =  constantData.m_ViewInv;
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

    {
        UserMarker marker( pCommandList, "simulation" );

        void* data = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS constantBuffer;
        constantBufferRing.AllocConstantBuffer( sizeof( simulationConstants ), &data, &constantBuffer );
        memcpy( data, &simulationConstants, sizeof( simulationConstants ) );


        pCommandList->SetComputeRootSignature( m_pSimulationRootSignature );
        pCommandList->SetComputeRootDescriptorTable( 0, m_SimulationUAVDescriptorTable.GetGPU() );
        pCommandList->SetComputeRootDescriptorTable( 1, m_SimulationSRVDescriptorTable.GetGPU() );
        pCommandList->SetComputeRootConstantBufferView( 2, constantBuffer );

        barriersBeforeSimulation.push_back( CD3DX12_RESOURCE_BARRIER::Transition( m_ParticleBufferA.GetResource(), m_ReadBufferStates, D3D12_RESOURCE_STATE_UNORDERED_ACCESS ) );
        barriersBeforeSimulation.push_back( CD3DX12_RESOURCE_BARRIER::Transition( m_PackedViewSpaceParticlePositions.GetResource(), m_ReadBufferStates, D3D12_RESOURCE_STATE_UNORDERED_ACCESS ) );
        barriersBeforeSimulation.push_back( CD3DX12_RESOURCE_BARRIER::Transition( m_MaxRadiusBuffer.GetResource(), m_ReadBufferStates, D3D12_RESOURCE_STATE_UNORDERED_ACCESS ) );
        barriersBeforeSimulation.push_back( CD3DX12_RESOURCE_BARRIER::Transition( m_AliveIndexBuffer.GetResource(), m_ReadBufferStates, D3D12_RESOURCE_STATE_UNORDERED_ACCESS ) );
        barriersBeforeSimulation.push_back( CD3DX12_RESOURCE_BARRIER::Transition( m_AliveCountBuffer.GetResource(), m_ReadBufferStates, D3D12_RESOURCE_STATE_UNORDERED_ACCESS ) );
        pCommandList->ResourceBarrier( (UINT)barriersBeforeSimulation.size(), &barriersBeforeSimulation[ 0 ] );
        m_ReadBufferStates = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

        // If we are resetting the particle system, then initialize the dead list
        if ( m_ResetSystem )
        {
            pCommandList->SetPipelineState( m_pResetParticlesPipeline );

            pCommandList->Dispatch( align( g_maxParticles, 256 ) / 256, 1, 1 );

            std::vector<D3D12_RESOURCE_BARRIER> barriersPostReset;
            barriersPostReset.push_back( CD3DX12_RESOURCE_BARRIER::UAV( m_ParticleBufferA.GetResource() ) );
            barriersPostReset.push_back( CD3DX12_RESOURCE_BARRIER::UAV( m_ParticleBufferB.GetResource() ) );
            barriersPostReset.push_back( CD3DX12_RESOURCE_BARRIER::UAV( m_DeadListBuffer.GetResource() ) );
            pCommandList->ResourceBarrier( (UINT)barriersPostReset.size(), &barriersPostReset[ 0 ] );

            m_ResetSystem = false;
        }

        // Emit particles into the system
        Emit( pCommandList, constantBufferRing, nNumEmitters, pEmitters );

        // Run the simulation for this frame
        Simulate( pCommandList );

        

        std::vector<D3D12_RESOURCE_BARRIER> barriersAfterSimulation;
        barriersAfterSimulation.push_back( CD3DX12_RESOURCE_BARRIER::Transition( m_ParticleBufferA.GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, SHADER_READ_STATE) );
        barriersAfterSimulation.push_back( CD3DX12_RESOURCE_BARRIER::Transition( m_PackedViewSpaceParticlePositions.GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, SHADER_READ_STATE) );
        barriersAfterSimulation.push_back( CD3DX12_RESOURCE_BARRIER::Transition( m_MaxRadiusBuffer.GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, SHADER_READ_STATE) );
        barriersAfterSimulation.push_back( CD3DX12_RESOURCE_BARRIER::Transition( m_AliveCountBuffer.GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, SHADER_READ_STATE) );
        barriersAfterSimulation.push_back( CD3DX12_RESOURCE_BARRIER::UAV( m_DeadListBuffer.GetResource() ) );
        pCommandList->ResourceBarrier( (UINT)barriersAfterSimulation.size(), &barriersAfterSimulation[ 0 ] );
    }

    // Conventional rasterization path
    {
        UserMarker marker( pCommandList, "rasterization" );

        // Sort if requested. Not doing so results in the particles rendering out of order and not blending correctly
        if ( flags & PF_Sort )
        {
            UserMarker marker( pCommandList, "sorting" );

            const D3D12_RESOURCE_BARRIER barriers[] =
            {
                CD3DX12_RESOURCE_BARRIER::UAV( m_AliveIndexBuffer.GetResource() ),
                CD3DX12_RESOURCE_BARRIER::UAV( m_AliveDistanceBuffer.GetResource() ),
            };
            pCommandList->ResourceBarrier( _countof( barriers ), barriers );

            Sort( pCommandList );
        }

        StreakMode streaks = flags & PF_Streaks ? StreaksOn : StreaksOff;
        ReactiveMode reactive = flags & PF_Reactive ? ReactiveOn : ReactiveOff;

        RenderingConstantBuffer* cb = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS renderingConstantBuffer;
        constantBufferRing.AllocConstantBuffer( sizeof( RenderingConstantBuffer ), (void**)&cb, &renderingConstantBuffer );
        cb->m_Projection = constantData.m_Projection;
        cb->m_ProjectionInv = simulationConstants.m_ProjectionInv;
        cb->m_SunColor = constantData.m_SunColor;
        cb->m_AmbientColor = constantData.m_AmbientColor;
        cb->m_SunDirectionVS = sunDirectionVS;
        cb->m_ScreenWidth = m_ScreenWidth;
        cb->m_ScreenHeight = m_ScreenHeight;

        pCommandList->SetGraphicsRootSignature( m_pRasterizationRootSignature );
        pCommandList->SetGraphicsRootDescriptorTable( 0, m_RasterizationSRVDescriptorTable.GetGPU() );
        pCommandList->SetGraphicsRootConstantBufferView( 1, renderingConstantBuffer );
        pCommandList->SetGraphicsRootUnorderedAccessView( 2, m_IndirectArgsBuffer.GetResource()->GetGPUVirtualAddress() );
        pCommandList->SetPipelineState( m_pRasterizationPipelines[ streaks ][ reactive ] );

        pCommandList->IASetIndexBuffer( &m_IndexBuffer );
        pCommandList->IASetVertexBuffers( 0, 0, nullptr );
        pCommandList->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

        std::vector<D3D12_RESOURCE_BARRIER> barriers;
        barriers.push_back( CD3DX12_RESOURCE_BARRIER::Transition( m_AliveIndexBuffer.GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, SHADER_READ_STATE) );
        barriers.push_back( CD3DX12_RESOURCE_BARRIER::Transition( m_IndirectArgsBuffer.GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT ) );
        pCommandList->ResourceBarrier( (UINT)barriers.size(), &barriers[ 0 ] );

        pCommandList->ExecuteIndirect( m_commandSignature, 1, m_IndirectArgsBuffer.GetResource(), 0, nullptr, 0 );

        pCommandList->ResourceBarrier( 1, &CD3DX12_RESOURCE_BARRIER::Transition( m_IndirectArgsBuffer.GetResource(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS ) );
    }

    m_ReadBufferStates = SHADER_READ_STATE;
}


void GPUParticleSystem::OnCreateDevice(Device &device, UploadHeap& uploadHeap, ResourceViewHeaps& heaps, StaticBufferPool& bufferPool, DynamicBufferRing& constantBufferRing )
{
    m_pDevice = &device;
    m_heaps = &heaps;
    
    m_ReadBufferStates = D3D12_RESOURCE_STATE_COMMON;
    m_WriteBufferStates = D3D12_RESOURCE_STATE_COMMON; // D3D12_RESOURCE_STATE_UNORDERED_ACCESS
    m_StridedBufferStates = D3D12_RESOURCE_STATE_COMMON;

    // Create the global particle pool. Each particle is split into two parts for better cache coherency. The first half contains the data more 
    // relevant to rendering while the second half is more related to simulation
    CD3DX12_RESOURCE_DESC RDescParticlesA = CD3DX12_RESOURCE_DESC::Buffer( sizeof( GPUParticlePartA ) * g_maxParticles, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS );
    m_ParticleBufferA.InitBuffer(&device, "ParticleBufferA", &RDescParticlesA, sizeof( GPUParticlePartA ), m_ReadBufferStates);

    CD3DX12_RESOURCE_DESC RDescParticlesB = CD3DX12_RESOURCE_DESC::Buffer( sizeof( GPUParticlePartB ) * g_maxParticles, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS );
    m_ParticleBufferB.InitBuffer(&device, "ParticleBufferB", &RDescParticlesB, sizeof( GPUParticlePartB ), m_WriteBufferStates);

    // The packed view space positions of particles are cached during simulation so allocate a buffer for them
    CD3DX12_RESOURCE_DESC RDescPackedViewSpaceParticlePositions = CD3DX12_RESOURCE_DESC::Buffer( 8 * g_maxParticles, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS );
    m_PackedViewSpaceParticlePositions.InitBuffer(&device, "PackedViewSpaceParticlePositions", &RDescPackedViewSpaceParticlePositions, 8, m_ReadBufferStates);

    // The maximum radii of each particle is cached during simulation to avoid recomputing multiple times later. This is only required
    // for streaked particles as they are not round so we cache the max radius of X and Y
    CD3DX12_RESOURCE_DESC RDescMaxRadiusBuffer = CD3DX12_RESOURCE_DESC::Buffer( sizeof( float ) * g_maxParticles, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS );
    m_MaxRadiusBuffer.InitBuffer(&device, "MaxRadiusBuffer", &RDescMaxRadiusBuffer, sizeof( float ), m_ReadBufferStates);

    // The dead particle index list. Created as an append buffer
    CD3DX12_RESOURCE_DESC RDescDeadListBuffer = CD3DX12_RESOURCE_DESC::Buffer( sizeof( INT ) * ( g_maxParticles + 1 ), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS );
    m_DeadListBuffer.InitBuffer(&device, "DeadListBuffer", &RDescDeadListBuffer, sizeof( INT ), m_WriteBufferStates);

    // Create the index buffer of alive particles that is to be sorted (at least in the rasterization path).
    // For the tiled rendering path this could be just a UINT index buffer as particles are not globally sorted
    CD3DX12_RESOURCE_DESC RDescAliveIndexBuffer = CD3DX12_RESOURCE_DESC::Buffer( sizeof( int ) * g_maxParticles, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS );
    m_AliveIndexBuffer.InitBuffer(&device, "AliveIndexBuffer", &RDescAliveIndexBuffer, sizeof( int ), m_ReadBufferStates);

    CD3DX12_RESOURCE_DESC RDescAliveDistanceBuffer = CD3DX12_RESOURCE_DESC::Buffer( sizeof( float ) * g_maxParticles, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS );
    m_AliveDistanceBuffer.InitBuffer(&device, "AliveDistanceBuffer", &RDescAliveDistanceBuffer, sizeof( float ), m_WriteBufferStates);

    // Create the single element buffer which is used to store the count of alive particles
    CD3DX12_RESOURCE_DESC RDescAliveCountBuffer = CD3DX12_RESOURCE_DESC::Buffer( sizeof( UINT ), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS );
    m_AliveCountBuffer.InitBuffer(&device, "AliveCountBuffer", &RDescAliveCountBuffer, sizeof( UINT ), m_ReadBufferStates);


    // Create the buffer to store the indirect args for the ExecuteIndirect call
    // Create the index buffer of alive particles that is to be sorted (at least in the rasterization path).
    CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer( sizeof( IndirectCommand ), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS );
    m_IndirectArgsBuffer.InitBuffer(&device, "IndirectArgsBuffer", &desc, sizeof( IndirectCommand ), m_WriteBufferStates);

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

    bufferPool.AllocIndexBuffer( g_maxParticles * 6, sizeof( UINT ), indices, &m_IndexBuffer );
    delete[] indices;

    // Initialize the random numbers texture
    FillRandomTexture( uploadHeap );

    m_Atlas.InitFromFile( &device, &uploadHeap, m_AtlasPath, true );

    CreateSimulationAssets();
    CreateRasterizedRenderingAssets();

    // Create the SortLib resources
    m_SortLib.OnCreate( m_pDevice, m_heaps, &constantBufferRing, &uploadHeap, &m_AliveCountBuffer, &m_AliveDistanceBuffer, &m_AliveIndexBuffer );
}

void GPUParticleSystem::CreateSimulationAssets()
{
    m_heaps->AllocCBV_SRV_UAVDescriptor( m_SimulationUAVDescriptorTableCount, &m_SimulationUAVDescriptorTable );

    m_ParticleBufferA.CreateBufferUAV( 0, nullptr, &m_SimulationUAVDescriptorTable );
    m_ParticleBufferB.CreateBufferUAV( 1, nullptr, &m_SimulationUAVDescriptorTable );
    m_DeadListBuffer.CreateBufferUAV( 2, nullptr, &m_SimulationUAVDescriptorTable );
    m_AliveIndexBuffer.CreateBufferUAV( 3, nullptr, &m_SimulationUAVDescriptorTable );
    m_AliveDistanceBuffer.CreateBufferUAV( 4, nullptr, &m_SimulationUAVDescriptorTable );
    m_MaxRadiusBuffer.CreateBufferUAV( 5, nullptr, &m_SimulationUAVDescriptorTable );
    m_PackedViewSpaceParticlePositions.CreateBufferUAV( 6, nullptr, &m_SimulationUAVDescriptorTable );
    m_IndirectArgsBuffer.CreateBufferUAV( 7, nullptr, &m_SimulationUAVDescriptorTable );
    m_AliveCountBuffer.CreateBufferUAV( 8, nullptr, &m_SimulationUAVDescriptorTable );

    m_heaps->AllocCBV_SRV_UAVDescriptor( m_SimulationSRVDescriptorTableCount, &m_SimulationSRVDescriptorTable );
    // depth buffer                                                     // t0
    m_RandomTexture.CreateSRV( 1, &m_SimulationSRVDescriptorTable );    // t1

    {
        CD3DX12_DESCRIPTOR_RANGE DescRange[2] = {};
        DescRange[0].Init( D3D12_DESCRIPTOR_RANGE_TYPE_UAV, m_SimulationUAVDescriptorTableCount, 0 );             // u0 - u8
        DescRange[1].Init( D3D12_DESCRIPTOR_RANGE_TYPE_SRV, m_SimulationSRVDescriptorTableCount, 0 );             // t0 - t1

        CD3DX12_ROOT_PARAMETER rootParamters[4] = {};
        rootParamters[0].InitAsDescriptorTable( 1, &DescRange[0], D3D12_SHADER_VISIBILITY_ALL ); // uavs
        rootParamters[1].InitAsDescriptorTable( 1, &DescRange[1], D3D12_SHADER_VISIBILITY_ALL ); // textures
        rootParamters[2].InitAsConstantBufferView( 0 ); // b0 - per frame
        rootParamters[3].InitAsConstantBufferView( 1 ); // b1 - per emitter

        CD3DX12_STATIC_SAMPLER_DESC sampler( 0, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP );

        CD3DX12_ROOT_SIGNATURE_DESC descRootSignature = {};
        descRootSignature.Init( _countof( rootParamters ), rootParamters, 1, &sampler );

        ID3DBlob *pOutBlob, *pErrorBlob = nullptr;
        D3D12SerializeRootSignature( &descRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &pOutBlob, &pErrorBlob );
        m_pDevice->GetDevice()->CreateRootSignature( 0, pOutBlob->GetBufferPointer(), pOutBlob->GetBufferSize(), IID_PPV_ARGS( &m_pSimulationRootSignature ) );
        m_pSimulationRootSignature->SetName( L"SimulationRootSignature" );

        pOutBlob->Release();
        if (pErrorBlob)
            pErrorBlob->Release();
    }

    D3D12_COMPUTE_PIPELINE_STATE_DESC descPso = {};
    descPso.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
    descPso.pRootSignature = m_pSimulationRootSignature;
    descPso.NodeMask = 0;

    DefineList defines;
    defines["API_DX12"] = "";

    {
        D3D12_SHADER_BYTECODE computeShader;
        CompileShaderFromFile( "ParticleSimulation.hlsl", &defines, "CS_Reset", "-T cs_6_0", &computeShader );

        descPso.CS = computeShader;
        m_pDevice->GetDevice()->CreateComputePipelineState( &descPso, IID_PPV_ARGS( &m_pResetParticlesPipeline ) );
        m_pResetParticlesPipeline->SetName( L"ResetParticles" );
    }

    {
        D3D12_SHADER_BYTECODE computeShader;
        CompileShaderFromFile( "ParticleSimulation.hlsl", &defines, "CS_Simulate", "-T cs_6_0", &computeShader );

        descPso.CS = computeShader;
        m_pDevice->GetDevice()->CreateComputePipelineState( &descPso, IID_PPV_ARGS( &m_pSimulatePipeline ) );
        m_pSimulatePipeline->SetName( L"Simulation" );
    }

    {
        D3D12_SHADER_BYTECODE computeShader;
        CompileShaderFromFile( "ParticleEmit.hlsl", &defines, "CS_Emit", "-T cs_6_0", &computeShader );

        descPso.CS = computeShader;
        m_pDevice->GetDevice()->CreateComputePipelineState( &descPso, IID_PPV_ARGS( &m_pEmitPipeline ) );
        m_pEmitPipeline->SetName( L"Emit" );
    }
}


void GPUParticleSystem::CreateRasterizedRenderingAssets()
{
    m_heaps->AllocCBV_SRV_UAVDescriptor( m_RasterizationSRVDescriptorTableCount, &m_RasterizationSRVDescriptorTable );
    m_ParticleBufferA.CreateSRV( 0, &m_RasterizationSRVDescriptorTable );
    m_PackedViewSpaceParticlePositions.CreateSRV( 1, &m_RasterizationSRVDescriptorTable );
    m_AliveCountBuffer.CreateSRV( 2, &m_RasterizationSRVDescriptorTable );
    m_AliveIndexBuffer.CreateSRV( 3, &m_RasterizationSRVDescriptorTable );
    m_Atlas.CreateSRV( 4, &m_RasterizationSRVDescriptorTable );
    // depth texture t5

    {
        CD3DX12_DESCRIPTOR_RANGE DescRange[1] = {};
        DescRange[0].Init( D3D12_DESCRIPTOR_RANGE_TYPE_SRV, m_RasterizationSRVDescriptorTableCount, 0 );             // t0-t5

        CD3DX12_ROOT_PARAMETER rootParamters[3] = {};
        rootParamters[0].InitAsDescriptorTable( 1, &DescRange[0], D3D12_SHADER_VISIBILITY_ALL ); // textures
        rootParamters[1].InitAsConstantBufferView( 0 ); // b0
        rootParamters[2].InitAsUnorderedAccessView( 0 );

        CD3DX12_STATIC_SAMPLER_DESC sampler( 0, D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP );

        CD3DX12_ROOT_SIGNATURE_DESC descRootSignature = {};
        descRootSignature.Init( _countof( rootParamters ), rootParamters, 1, &sampler );

        ID3DBlob *pOutBlob, *pErrorBlob = nullptr;
        D3D12SerializeRootSignature( &descRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &pOutBlob, &pErrorBlob );
        m_pDevice->GetDevice()->CreateRootSignature( 0, pOutBlob->GetBufferPointer(), pOutBlob->GetBufferSize(), IID_PPV_ARGS( &m_pRasterizationRootSignature ) );
        m_pRasterizationRootSignature->SetName( L"RasterizationRootSignature" );

        pOutBlob->Release();
        if (pErrorBlob)
            pErrorBlob->Release();
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC descPso = {};
    descPso.InputLayout = { nullptr, 0 };
    descPso.pRootSignature = m_pRasterizationRootSignature;

    descPso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    descPso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    descPso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    descPso.BlendState.IndependentBlendEnable = true;
    descPso.BlendState.RenderTarget[0].BlendEnable = true;
    descPso.BlendState.RenderTarget[2].BlendEnable = true;

    descPso.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    descPso.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    descPso.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    descPso.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    descPso.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
    descPso.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;

    descPso.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    descPso.BlendState.RenderTarget[1].RenderTargetWriteMask = 0;
    descPso.BlendState.RenderTarget[2].RenderTargetWriteMask = 0;
    descPso.BlendState.RenderTarget[3].RenderTargetWriteMask = 0;

    descPso.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    descPso.DepthStencilState.DepthEnable = TRUE;
    descPso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    descPso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
    descPso.SampleMask = UINT_MAX;
    descPso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    descPso.NumRenderTargets = 4;
    descPso.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
    descPso.RTVFormats[1] = DXGI_FORMAT_R16G16_FLOAT;
    descPso.RTVFormats[2] = DXGI_FORMAT_R8_UNORM;
    descPso.RTVFormats[3] = DXGI_FORMAT_R8_UNORM;
    descPso.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    descPso.SampleDesc.Count = 1;
    descPso.NodeMask = 0;

    for ( int i = 0; i < NumStreakModes; i++ )
    {
        for ( int j = 0; j < NumReactiveModes; j++ )
        {
            descPso.BlendState.RenderTarget[2].RenderTargetWriteMask = 0;

            DefineList defines;
            defines["API_DX12"] = "";
            if ( i == StreaksOn )
                defines["STREAKS"] = "";

            if ( j == ReactiveOn )
            {
                defines["REACTIVE"] = "";
                descPso.BlendState.RenderTarget[2].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_RED;
            }

            D3D12_SHADER_BYTECODE vertexShader = {};
            CompileShaderFromFile( "ParticleRender.hlsl", &defines, "VS_StructuredBuffer", "-T vs_6_0", &vertexShader );

            D3D12_SHADER_BYTECODE pixelShader = {};
            CompileShaderFromFile( "ParticleRender.hlsl", &defines, "PS_Billboard", "-T ps_6_0", &pixelShader );

            descPso.VS = vertexShader;
            descPso.PS = pixelShader;
            m_pDevice->GetDevice()->CreateGraphicsPipelineState( &descPso, IID_PPV_ARGS( &m_pRasterizationPipelines[ i ][ j ] ) );
        }
    }

    D3D12_INDIRECT_ARGUMENT_DESC argumentDescs[2] = {};
    argumentDescs[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_UNORDERED_ACCESS_VIEW;
    argumentDescs[0].UnorderedAccessView.RootParameterIndex = 2;
    argumentDescs[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

    D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc = {};
    commandSignatureDesc.pArgumentDescs = argumentDescs;
    commandSignatureDesc.NumArgumentDescs = _countof( argumentDescs );
    commandSignatureDesc.ByteStride = sizeof( IndirectCommand );

    m_pDevice->GetDevice()->CreateCommandSignature( &commandSignatureDesc, m_pRasterizationRootSignature, IID_PPV_ARGS( &m_commandSignature ) );
    m_commandSignature->SetName( L"CommandSignature" );
}


void GPUParticleSystem::OnResizedSwapChain( int width, int height, Texture& depthBuffer )
{
    m_ScreenWidth = width;
    m_ScreenHeight = height;
    m_InvScreenWidth = 1.0f / m_ScreenWidth;
    m_InvScreenHeight = 1.0f / m_ScreenHeight;

    depthBuffer.CreateSRV( 0, &m_SimulationSRVDescriptorTable );
    depthBuffer.CreateSRV( 5, &m_RasterizationSRVDescriptorTable );
}


void GPUParticleSystem::OnReleasingSwapChain()
{
}


void GPUParticleSystem::OnDestroyDevice()
{
    m_pDevice = nullptr;

    m_ParticleBufferA.OnDestroy();
    m_ParticleBufferB.OnDestroy();
    m_PackedViewSpaceParticlePositions.OnDestroy();
    m_MaxRadiusBuffer.OnDestroy();
    m_DeadListBuffer.OnDestroy();
    m_AliveIndexBuffer.OnDestroy();
    m_AliveDistanceBuffer.OnDestroy();
    m_AliveCountBuffer.OnDestroy();
    m_RandomTexture.OnDestroy();
    m_Atlas.OnDestroy();
    m_IndirectArgsBuffer.OnDestroy();

    m_pSimulatePipeline->Release();
    m_pSimulatePipeline = nullptr;

    m_pResetParticlesPipeline->Release();
    m_pResetParticlesPipeline = nullptr;

    m_pEmitPipeline->Release();
    m_pEmitPipeline = nullptr;

    m_pSimulationRootSignature->Release();
    m_pSimulationRootSignature = nullptr;

    for ( int i = 0; i < NumStreakModes; i++ )
    {
        for ( int j = 0; j < NumReactiveModes; j++ )
        {
            m_pRasterizationPipelines[ i ][ j ]->Release();
            m_pRasterizationPipelines[ i ][ j ] = nullptr;
        }
    }

    m_pRasterizationRootSignature->Release();
    m_pRasterizationRootSignature = nullptr;

    m_commandSignature->Release();
    m_commandSignature = nullptr;

    m_SortLib.OnDestroy();

    m_ResetSystem = true;
}


// Per-frame emission of particles into the GPU simulation
void GPUParticleSystem::Emit( ID3D12GraphicsCommandList* pCommandList, DynamicBufferRing& constantBufferRing, int numEmitters, const EmitterParams* emitters )
{
    pCommandList->SetPipelineState( m_pEmitPipeline );

    // Run CS for each emitter
    for ( int i = 0; i < numEmitters; i++ )
    {
        const EmitterParams& emitter = emitters[ i ];

        if ( emitter.m_NumToEmit > 0 )
        {
            EmitterConstantBuffer* constants = nullptr;
            D3D12_GPU_VIRTUAL_ADDRESS constantBuffer;
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
            pCommandList->SetComputeRootConstantBufferView( 3, constantBuffer );

            // Dispatch enough thread groups to spawn the requested particles
            int numThreadGroups = align( emitter.m_NumToEmit, 1024 ) / 1024;
            pCommandList->Dispatch( numThreadGroups, 1, 1 );

            pCommandList->ResourceBarrier( 1, &CD3DX12_RESOURCE_BARRIER::UAV( m_DeadListBuffer.GetResource() ) );
        }
    }

    // RaW barriers
    pCommandList->ResourceBarrier( 1, &CD3DX12_RESOURCE_BARRIER::UAV( m_ParticleBufferA.GetResource() ) );
    pCommandList->ResourceBarrier( 1, &CD3DX12_RESOURCE_BARRIER::UAV( m_ParticleBufferB.GetResource() ) );
}


// Per-frame simulation step
void GPUParticleSystem::Simulate( ID3D12GraphicsCommandList* pCommandList )
{
    pCommandList->SetPipelineState( m_pSimulatePipeline );
    pCommandList->Dispatch( align( g_maxParticles, 256 ) / 256, 1, 1 );
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

    m_RandomTexture.InitFromData(m_pDevice, "RadomTexture", uploadHeap, header, values );

    delete[] values;
}
