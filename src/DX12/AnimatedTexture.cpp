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


void AnimatedTextures::OnCreate( Device& device, UploadHeap& uploadHeap, StaticBufferPool& bufferPool, ResourceViewHeaps& resourceViewHeaps, DynamicBufferRing& constantBufferRing )
{
    m_pResourceViewHeaps = &resourceViewHeaps;
    m_constantBufferRing = &constantBufferRing;

    D3D12_SHADER_BYTECODE vs = {};
    D3D12_SHADER_BYTECODE ps = {};
    CompileShaderFromFile( "AnimatedTexture.hlsl", nullptr, "VSMain", "-T vs_6_0", &vs );
    CompileShaderFromFile( "AnimatedTexture.hlsl", nullptr, "PSMain", "-T ps_6_0", &ps );

    CD3DX12_DESCRIPTOR_RANGE DescRange[1] = {};
    DescRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0 );             // t0

    CD3DX12_ROOT_PARAMETER rootParamters[2] = {};
    rootParamters[0].InitAsDescriptorTable( 1, &DescRange[0], D3D12_SHADER_VISIBILITY_PIXEL ); // textures
    rootParamters[1].InitAsConstantBufferView( 0, 0, D3D12_SHADER_VISIBILITY_ALL );

    CD3DX12_STATIC_SAMPLER_DESC sampler( 0 );
    CD3DX12_ROOT_SIGNATURE_DESC descRootSignature = CD3DX12_ROOT_SIGNATURE_DESC();
    descRootSignature.Init( _countof(rootParamters), rootParamters, 1, &sampler );

    // deny uneccessary access to certain pipeline stages   
    descRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
                | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS
                | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS
                | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

    ID3DBlob *pOutBlob, *pErrorBlob = NULL;
    ThrowIfFailed(D3D12SerializeRootSignature(&descRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &pOutBlob, &pErrorBlob));
    ThrowIfFailed(device.GetDevice()->CreateRootSignature(0, pOutBlob->GetBufferPointer(), pOutBlob->GetBufferSize(), IID_PPV_ARGS(&m_pRootSignature)));
    SetName( m_pRootSignature, "AnimatedTexture" );

    pOutBlob->Release();
    if (pErrorBlob)
        pErrorBlob->Release();

    D3D12_GRAPHICS_PIPELINE_STATE_DESC descPso = {};
    descPso.pRootSignature = m_pRootSignature;
    descPso.VS = vs;
    descPso.PS = ps;
    descPso.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    descPso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
    descPso.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    descPso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    descPso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    descPso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    descPso.BlendState.IndependentBlendEnable = true;
    descPso.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    descPso.BlendState.RenderTarget[1].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN;
    descPso.BlendState.RenderTarget[2].RenderTargetWriteMask = 0x0;
    descPso.BlendState.RenderTarget[3].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_RED;
    descPso.SampleMask = UINT_MAX;
    descPso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    descPso.NumRenderTargets = 4;
    descPso.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
    descPso.RTVFormats[1] = DXGI_FORMAT_R16G16_FLOAT;
    descPso.RTVFormats[2] = DXGI_FORMAT_R8_UNORM;
    descPso.RTVFormats[3] = DXGI_FORMAT_R8_UNORM;
    descPso.SampleDesc.Count = 1;

    ThrowIfFailed(device.GetDevice()->CreateGraphicsPipelineState(&descPso, IID_PPV_ARGS(&m_pPipelines[0])));
    SetName(m_pPipelines[0], "AnimatedTexturePipelineComp");

    descPso.BlendState.RenderTarget[3].RenderTargetWriteMask = 0;
    ThrowIfFailed(device.GetDevice()->CreateGraphicsPipelineState(&descPso, IID_PPV_ARGS(&m_pPipelines[1])));
    SetName(m_pPipelines[1], "AnimatedTexturePipelineNoComp");

    UINT indices[6] = { 0, 1, 2, 2, 1, 3 };
    bufferPool.AllocIndexBuffer( _countof( indices ), sizeof( UINT ), indices, &m_indexBuffer );

    resourceViewHeaps.AllocCBV_SRV_UAVDescriptor( _countof( m_textures ), &m_descriptorTable );

    m_textures[0].InitFromFile( &device, &uploadHeap, "..\\media\\lion.jpg", true );
    m_textures[1].InitFromFile( &device, &uploadHeap, "..\\media\\checkerboard.dds", true );
    m_textures[2].InitFromFile( &device, &uploadHeap, "..\\media\\composition_text.dds", true );

    for ( int i = 0; i < _countof( m_textures ); i++ )
    {
        m_textures[ i ].CreateSRV( i, &m_descriptorTable );
    }
}


void AnimatedTextures::OnDestroy()
{
    for ( int i = 0; i < _countof( m_textures ); i++ )
    {
        m_textures[i].OnDestroy();
    }

    for ( int i = 0; i < _countof( m_pPipelines ); i++ )
    {
        m_pPipelines[i]->Release();
        m_pPipelines[i] = nullptr;
    }

    m_pRootSignature->Release();
    m_pRootSignature = nullptr;
    m_pResourceViewHeaps = nullptr;
}


void AnimatedTextures::Render( ID3D12GraphicsCommandList* pCommandList, float frameTime, float speed, bool compositionMask, const Camera& camera )
{
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

    D3D12_GPU_VIRTUAL_ADDRESS cb = {};
    ConstantBuffer* constantBuffer = nullptr;
    m_constantBufferRing->AllocConstantBuffer( sizeof(*constantBuffer), (void**)&constantBuffer, &cb );

    constantBuffer->currentViewProj = camera.GetProjection() * camera.GetView();
    constantBuffer->previousViewProj = camera.GetPrevProjection() * camera.GetPrevView();

    constantBuffer->jitterCompensation[0] = camera.GetPrevProjection().getCol2().getX() - camera.GetProjection().getCol2().getX();
    constantBuffer->jitterCompensation[1] = camera.GetPrevProjection().getCol2().getY() - camera.GetProjection().getCol2().getY();
    constantBuffer->scrollFactor = m_scrollFactor;
    constantBuffer->rotationFactor = m_rotationFactor;
    constantBuffer->mode = textureIndex;

    ID3D12DescriptorHeap* descriptorHeaps[] = { m_pResourceViewHeaps->GetCBV_SRV_UAVHeap(), m_pResourceViewHeaps->GetSamplerHeap() };
    pCommandList->SetDescriptorHeaps( _countof( descriptorHeaps ), descriptorHeaps );
    pCommandList->SetGraphicsRootSignature( m_pRootSignature );
    pCommandList->SetGraphicsRootDescriptorTable( 0, m_descriptorTable.GetGPU( textureIndex ) );
    pCommandList->SetGraphicsRootConstantBufferView( 1, cb );

    pCommandList->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
    pCommandList->IASetIndexBuffer( &m_indexBuffer );
    pCommandList->IASetVertexBuffers( 0, 0, nullptr );
    pCommandList->SetPipelineState( m_pPipelines[compositionMask ? 0 : 1] );
    pCommandList->DrawIndexedInstanced( 6, 2, 0, 0, 0 );
}

