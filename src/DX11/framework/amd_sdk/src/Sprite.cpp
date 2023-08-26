//
// Copyright (c) 2017 Advanced Micro Devices, Inc. All rights reserved.
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

//--------------------------------------------------------------------------------------
// File: Sprite.cpp
//
// Sprite class definition. This class provides functionality to render sprites, at a
// given position and scale.
//--------------------------------------------------------------------------------------

// DXUT helper code
#include "DXUT.h"
#include "SDKmisc.h"
#include "SDKMesh.h"

#include "Sprite.h"
#include "HelperFunctions.h"

using namespace AMD;

struct SpriteVertex
{
    DirectX::XMFLOAT3 v3Pos;
    DirectX::XMFLOAT2 v2TexCoord;
};

struct SpriteBorderVertex
{
    DirectX::XMFLOAT3 v3Pos;
};

// Constant buffer layout for transfering data to the sprite HLSL functions
struct CB_SPRITE
{
    float   fViewportSize[4];
    float   fPlotParams[4];
    float   fTextureSize[4];
    float   fDepthRange[4];
    float   fSpriteColor[4];
    float   fBorderColor[4];
    float   fSampleIndex[4];
};
static CB_SPRITE s_CBSprite;


//--------------------------------------------------------------------------------------
// Constructor
//--------------------------------------------------------------------------------------
Sprite::Sprite()
{
    m_pVertexLayout = NULL;
    m_pVertexBuffer = NULL;
    m_pcbSprite = NULL;

    memset( &s_CBSprite, 0, sizeof( CB_SPRITE ) );

    m_pSpriteVS = NULL;
    m_pSpriteBorderVS = NULL;
    m_pSpritePS = NULL;
    m_pSpriteMSPS = NULL;
    m_pSpriteAsDepthPS = NULL;
    m_pSpriteAsDepthMSPS = NULL;
    m_pSpriteBorderPS = NULL;
    m_pSpriteUntexturedPS = NULL;

    m_EnableScissorTest = false;
    m_PointSampleMode = true;
    m_pSamplePoint = NULL;
    m_pSampleLinear = NULL;
    m_pRasterState = NULL;
    m_pRasterStateWithScissor = NULL;
    m_pEnableCulling = NULL;
    m_pNoBlending = NULL;
    m_pSrcAlphaBlending = NULL;
    m_pDisableDepthTestWrite = NULL;
    m_pEnableDepthTestWrite = NULL;

    SetSpriteColor( DirectX::XMVectorSet( 1.0f, 1.0f, 1.0f, 1.0f ) );
    SetBorderColor( DirectX::XMVectorSet( 0.0f, 0.0f, 0.0f, 1.0f ) );
}


//--------------------------------------------------------------------------------------
// Destructor
//--------------------------------------------------------------------------------------
Sprite::~Sprite()
{
}


//--------------------------------------------------------------------------------------
// Hook method
//--------------------------------------------------------------------------------------
HRESULT Sprite::OnCreateDevice( ID3D11Device* pd3dDevice )
{
    HRESULT hr;

    // Check we have a valid device pointer
    assert( NULL != pd3dDevice );

    // Setup shaders
    ID3DBlob* pBlob = NULL;

    // VS 1
    hr = AMD::CompileShaderFromFile( L"ShaderLibDX\\dx11\\Sprite.hlsl", "VsSprite", "vs_4_0", &pBlob, NULL );
    assert( D3D_OK == hr );
    hr = pd3dDevice->CreateVertexShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &m_pSpriteVS );
    assert( D3D_OK == hr );
    // Define the input layout
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXTURE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    hr = pd3dDevice->CreateInputLayout( layout, ARRAYSIZE( layout ), pBlob->GetBufferPointer(),
        pBlob->GetBufferSize(), &m_pVertexLayout );
    assert( D3D_OK == hr );
    SAFE_RELEASE( pBlob );

    // VS 2
    hr = AMD::CompileShaderFromFile( L"ShaderLibDX\\dx11\\Sprite.hlsl", "VsSpriteBorder", "vs_4_0", &pBlob, NULL );
    assert( D3D_OK == hr );
    hr = pd3dDevice->CreateVertexShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &m_pSpriteBorderVS );
    assert( D3D_OK == hr );
    // Define the input layout
    D3D11_INPUT_ELEMENT_DESC BorderLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    hr = pd3dDevice->CreateInputLayout( BorderLayout, ARRAYSIZE( BorderLayout ), pBlob->GetBufferPointer(),
        pBlob->GetBufferSize(), &m_pBorderVertexLayout );
    assert( D3D_OK == hr );
    SAFE_RELEASE( pBlob );

    // PS 1
    hr = AMD::CompileShaderFromFile( L"ShaderLibDX\\dx11\\Sprite.hlsl", "PsSprite", "ps_4_0", &pBlob, NULL );
    assert( D3D_OK == hr );
    hr = pd3dDevice->CreatePixelShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &m_pSpritePS );
    assert( D3D_OK == hr );
    SAFE_RELEASE( pBlob );

    // PS 2
    hr = AMD::CompileShaderFromFile( L"ShaderLibDX\\dx11\\Sprite.hlsl", "PsSpriteMS", "ps_4_0", &pBlob, NULL );
    assert( D3D_OK == hr );
    hr = pd3dDevice->CreatePixelShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &m_pSpriteMSPS );
    assert( D3D_OK == hr );
    SAFE_RELEASE( pBlob );

    // PS 3
    hr = AMD::CompileShaderFromFile( L"ShaderLibDX\\dx11\\Sprite.hlsl", "PsSpriteAsDepth", "ps_4_0", &pBlob, NULL );
    assert( D3D_OK == hr );
    hr = pd3dDevice->CreatePixelShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &m_pSpriteAsDepthPS );
    assert( D3D_OK == hr );
    SAFE_RELEASE( pBlob );

    // PS 4
    hr = AMD::CompileShaderFromFile( L"ShaderLibDX\\dx11\\Sprite.hlsl", "PsSpriteAsDepthMS", "ps_4_0", &pBlob, NULL );
    assert( D3D_OK == hr );
    hr = pd3dDevice->CreatePixelShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &m_pSpriteAsDepthMSPS );
    assert( D3D_OK == hr );
    SAFE_RELEASE( pBlob );

    // PS 5
    hr = AMD::CompileShaderFromFile( L"ShaderLibDX\\dx11\\Sprite.hlsl", "PsSpriteBorder", "ps_4_0", &pBlob, NULL );
    assert( D3D_OK == hr );
    hr = pd3dDevice->CreatePixelShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &m_pSpriteBorderPS );
    assert( D3D_OK == hr );
    SAFE_RELEASE( pBlob );

    hr = AMD::CompileShaderFromFile( L"ShaderLibDX\\dx11\\Sprite.hlsl", "PsSpriteUntextured", "ps_4_0", &pBlob, NULL );
    assert( D3D_OK == hr );
    hr = pd3dDevice->CreatePixelShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &m_pSpriteUntexturedPS );
    assert( D3D_OK == hr );
    SAFE_RELEASE( pBlob );

    hr = AMD::CompileShaderFromFile( L"ShaderLibDX\\dx11\\Sprite.hlsl", "PsSpriteVolume", "ps_4_0", &pBlob, NULL );
    assert( D3D_OK == hr );
    hr = pd3dDevice->CreatePixelShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &m_pSpriteVolumePS );
    assert( D3D_OK == hr );
    SAFE_RELEASE( pBlob );

    ///////////////////////////////////////////////////////////////////////////
    // Setup VB for the textured sprite
    ///////////////////////////////////////////////////////////////////////////

    // Fill out a unit quad
    SpriteVertex QuadVertices[6];
    QuadVertices[0].v3Pos = DirectX::XMFLOAT3( 0.0f, -1.0f, 0.5f );
    QuadVertices[0].v2TexCoord = DirectX::XMFLOAT2( 0.0f, 0.0f );
    QuadVertices[1].v3Pos = DirectX::XMFLOAT3( 0.0f, 0.0f, 0.5f );
    QuadVertices[1].v2TexCoord = DirectX::XMFLOAT2( 0.0f, 1.0f );
    QuadVertices[2].v3Pos = DirectX::XMFLOAT3( 1.0f, -1.0f, 0.5f );
    QuadVertices[2].v2TexCoord = DirectX::XMFLOAT2( 1.0f, 0.0f );
    QuadVertices[3].v3Pos = DirectX::XMFLOAT3( 0.0f, 0.0f, 0.5f );
    QuadVertices[3].v2TexCoord = DirectX::XMFLOAT2( 0.0f, 1.0f );
    QuadVertices[4].v3Pos = DirectX::XMFLOAT3( 1.0f, 0.0f, 0.5f );
    QuadVertices[4].v2TexCoord = DirectX::XMFLOAT2( 1.0f, 1.0f );
    QuadVertices[5].v3Pos = DirectX::XMFLOAT3( 1.0f, -1.0f, 0.5f );
    QuadVertices[5].v2TexCoord = DirectX::XMFLOAT2( 1.0f, 0.0f );

    // Create the vertex buffer
    D3D11_BUFFER_DESC BD;
    ZeroMemory( &BD, sizeof( BD ) );
    BD.Usage = D3D11_USAGE_DYNAMIC;
    BD.ByteWidth = sizeof( SpriteVertex ) * 6;
    BD.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    BD.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    BD.MiscFlags = 0;
    D3D11_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = QuadVertices;
    hr = pd3dDevice->CreateBuffer( &BD, &InitData, &m_pVertexBuffer );
    assert( D3D_OK == hr );

    ///////////////////////////////////////////////////////////////////////////
    // Setup VB for the sprite border
    ///////////////////////////////////////////////////////////////////////////

    // Fill out a unit quad
    SpriteBorderVertex QuadBorderVertices[5];
    QuadBorderVertices[0].v3Pos = DirectX::XMFLOAT3( 0.0f, -1.0f, 0.5f );
    QuadBorderVertices[1].v3Pos = DirectX::XMFLOAT3( 0.0f, 0.0f, 0.5f );
    QuadBorderVertices[2].v3Pos = DirectX::XMFLOAT3( 1.0f, 0.0f, 0.5f );
    QuadBorderVertices[3].v3Pos = DirectX::XMFLOAT3( 1.0f, -1.0f, 0.5f );
    QuadBorderVertices[4].v3Pos = DirectX::XMFLOAT3( 0.0f, -1.0f, 0.5f );

    // Create the vertex buffer
    BD.Usage = D3D11_USAGE_DEFAULT;
    BD.ByteWidth = sizeof( SpriteBorderVertex ) * 5;
    BD.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    BD.CPUAccessFlags = 0;
    BD.MiscFlags = 0;
    InitData.pSysMem = QuadBorderVertices;
    hr = pd3dDevice->CreateBuffer( &BD, &InitData, &m_pBorderVertexBuffer );
    assert( D3D_OK == hr );

    // Setup constant buffer
    D3D11_BUFFER_DESC Desc;
    ZeroMemory( &Desc, sizeof( Desc ) );
    Desc.Usage = D3D11_USAGE_DYNAMIC;
    Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Desc.MiscFlags = 0;
    Desc.ByteWidth = sizeof( CB_SPRITE );
    hr = pd3dDevice->CreateBuffer( &Desc, NULL, &m_pcbSprite );
    assert( D3D_OK == hr );

    // Create sampler states for point and linear
    // Point
    D3D11_SAMPLER_DESC SamDesc;
    SamDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    SamDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    SamDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    SamDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    SamDesc.MipLODBias = 0.0f;
    SamDesc.MaxAnisotropy = 1;
    SamDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    SamDesc.BorderColor[0] = SamDesc.BorderColor[1] = SamDesc.BorderColor[2] = SamDesc.BorderColor[3] = 0;
    SamDesc.MinLOD = 0;
    SamDesc.MaxLOD = D3D11_FLOAT32_MAX;
    hr = pd3dDevice->CreateSamplerState( &SamDesc, &m_pSamplePoint );
    assert( D3D_OK == hr );
    // Linear
    SamDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    SamDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    SamDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    SamDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    hr = pd3dDevice->CreateSamplerState( &SamDesc, &m_pSampleLinear );
    assert( D3D_OK == hr );

    // Disable culling
    D3D11_RASTERIZER_DESC RasterizerDesc;
    RasterizerDesc.FillMode = D3D11_FILL_SOLID;
    RasterizerDesc.CullMode = D3D11_CULL_NONE;
    RasterizerDesc.FrontCounterClockwise = FALSE;
    RasterizerDesc.DepthBias = 0;
    RasterizerDesc.DepthBiasClamp = 0.0f;
    RasterizerDesc.SlopeScaledDepthBias = 0.0f;
    RasterizerDesc.DepthClipEnable = TRUE;
    RasterizerDesc.ScissorEnable = FALSE;
    RasterizerDesc.MultisampleEnable = FALSE;
    RasterizerDesc.AntialiasedLineEnable = FALSE;
    hr = pd3dDevice->CreateRasterizerState( &RasterizerDesc, &m_pRasterState );
    assert( D3D_OK == hr );

    RasterizerDesc.ScissorEnable = TRUE;
    hr = pd3dDevice->CreateRasterizerState( &RasterizerDesc, &m_pRasterStateWithScissor );
    assert( D3D_OK == hr );

    // Enable culling
    RasterizerDesc.ScissorEnable = FALSE;
    RasterizerDesc.CullMode = D3D11_CULL_BACK;
    hr = pd3dDevice->CreateRasterizerState( &RasterizerDesc, &m_pEnableCulling );
    assert( D3D_OK == hr );

    // No Blending
    D3D11_BLEND_DESC BlendDesc;
    BlendDesc.AlphaToCoverageEnable = FALSE;
    BlendDesc.IndependentBlendEnable = FALSE;
    D3D11_RENDER_TARGET_BLEND_DESC RTBlendDesc;
    RTBlendDesc.BlendEnable = FALSE;
    RTBlendDesc.SrcBlend = D3D11_BLEND_ONE;
    RTBlendDesc.DestBlend = D3D11_BLEND_ZERO;
    RTBlendDesc.BlendOp = D3D11_BLEND_OP_ADD;
    RTBlendDesc.SrcBlendAlpha = D3D11_BLEND_ONE;
    RTBlendDesc.DestBlendAlpha = D3D11_BLEND_ZERO;
    RTBlendDesc.BlendOpAlpha = D3D11_BLEND_OP_ADD;
    RTBlendDesc.RenderTargetWriteMask = 0x0F;
    BlendDesc.RenderTarget[0] = RTBlendDesc;
    BlendDesc.RenderTarget[1] = RTBlendDesc;
    BlendDesc.RenderTarget[2] = RTBlendDesc;
    BlendDesc.RenderTarget[3] = RTBlendDesc;
    BlendDesc.RenderTarget[4] = RTBlendDesc;
    BlendDesc.RenderTarget[5] = RTBlendDesc;
    BlendDesc.RenderTarget[6] = RTBlendDesc;
    BlendDesc.RenderTarget[7] = RTBlendDesc;
    hr = pd3dDevice->CreateBlendState( &BlendDesc, &m_pNoBlending );
    assert( D3D_OK == hr );

    // SrcAlphaBlending
    BlendDesc.AlphaToCoverageEnable = FALSE;
    BlendDesc.IndependentBlendEnable = FALSE;
    RTBlendDesc.BlendEnable = TRUE;
    RTBlendDesc.SrcBlend = D3D11_BLEND_SRC_ALPHA;
    RTBlendDesc.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    RTBlendDesc.BlendOp = D3D11_BLEND_OP_ADD;
    RTBlendDesc.SrcBlendAlpha = D3D11_BLEND_ZERO;
    RTBlendDesc.DestBlendAlpha = D3D11_BLEND_ZERO;
    RTBlendDesc.BlendOpAlpha = D3D11_BLEND_OP_ADD;
    RTBlendDesc.RenderTargetWriteMask = 0x0F;
    BlendDesc.RenderTarget[0] = RTBlendDesc;
    BlendDesc.RenderTarget[1] = RTBlendDesc;
    BlendDesc.RenderTarget[2] = RTBlendDesc;
    BlendDesc.RenderTarget[3] = RTBlendDesc;
    BlendDesc.RenderTarget[4] = RTBlendDesc;
    BlendDesc.RenderTarget[5] = RTBlendDesc;
    BlendDesc.RenderTarget[6] = RTBlendDesc;
    BlendDesc.RenderTarget[7] = RTBlendDesc;
    hr = pd3dDevice->CreateBlendState( &BlendDesc, &m_pSrcAlphaBlending );
    assert( D3D_OK == hr );

    // Disable depth test write
    D3D11_DEPTH_STENCIL_DESC DepthStencilDesc;
    DepthStencilDesc.DepthEnable = FALSE;
    DepthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    DepthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;
    DepthStencilDesc.StencilEnable = FALSE;
    DepthStencilDesc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
    DepthStencilDesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
    hr = pd3dDevice->CreateDepthStencilState( &DepthStencilDesc, &m_pDisableDepthTestWrite );
    assert( D3D_OK == hr );

    // Disable depth test write
    DepthStencilDesc.DepthEnable = TRUE;
    DepthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    hr = pd3dDevice->CreateDepthStencilState( &DepthStencilDesc, &m_pEnableDepthTestWrite );
    assert( D3D_OK == hr );

    return hr;
}


//--------------------------------------------------------------------------------------
// Hook method
//--------------------------------------------------------------------------------------
void Sprite::OnDestroyDevice()
{
    SAFE_RELEASE( m_pVertexLayout );
    SAFE_RELEASE( m_pVertexBuffer );
    SAFE_RELEASE( m_pBorderVertexLayout );
    SAFE_RELEASE( m_pBorderVertexBuffer );

    SAFE_RELEASE( m_pcbSprite );

    SAFE_RELEASE( m_pSpriteVS );
    SAFE_RELEASE( m_pSpriteBorderVS );
    SAFE_RELEASE( m_pSpritePS );
    SAFE_RELEASE( m_pSpriteMSPS );
    SAFE_RELEASE( m_pSpriteAsDepthPS );
    SAFE_RELEASE( m_pSpriteAsDepthMSPS );
    SAFE_RELEASE( m_pSpriteBorderPS );
    SAFE_RELEASE( m_pSpriteUntexturedPS );
    SAFE_RELEASE( m_pSpriteVolumePS );

    SAFE_RELEASE( m_pSamplePoint );
    SAFE_RELEASE( m_pSampleLinear );
    SAFE_RELEASE( m_pRasterState );
    SAFE_RELEASE( m_pRasterStateWithScissor );
    SAFE_RELEASE( m_pEnableCulling );
    SAFE_RELEASE( m_pNoBlending );
    SAFE_RELEASE( m_pSrcAlphaBlending );
    SAFE_RELEASE( m_pDisableDepthTestWrite );
    SAFE_RELEASE( m_pEnableDepthTestWrite );
}


//--------------------------------------------------------------------------------------
// Hook method
//--------------------------------------------------------------------------------------
void Sprite::OnResizedSwapChain( const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc )
{
    assert( NULL != pBackBufferSurfaceDesc );

    s_CBSprite.fViewportSize[0] = (float)pBackBufferSurfaceDesc->Width;
    s_CBSprite.fViewportSize[1] = (float)pBackBufferSurfaceDesc->Height;
}


//--------------------------------------------------------------------------------------
// Sets the sprite color
//--------------------------------------------------------------------------------------
void Sprite::SetSpriteColor( DirectX::XMVECTOR Color )
{
    s_CBSprite.fSpriteColor[0] = DirectX::XMVectorGetX( Color );
    s_CBSprite.fSpriteColor[1] = DirectX::XMVectorGetY( Color );
    s_CBSprite.fSpriteColor[2] = DirectX::XMVectorGetZ( Color );
    s_CBSprite.fSpriteColor[3] = DirectX::XMVectorGetW( Color );
}


//--------------------------------------------------------------------------------------
// Sets the border color for a sprite
//--------------------------------------------------------------------------------------
void Sprite::SetBorderColor( DirectX::XMVECTOR Color )
{
    s_CBSprite.fBorderColor[0] = DirectX::XMVectorGetX( Color );
    s_CBSprite.fBorderColor[1] = DirectX::XMVectorGetY( Color );
    s_CBSprite.fBorderColor[2] = DirectX::XMVectorGetZ( Color );
    s_CBSprite.fBorderColor[3] = DirectX::XMVectorGetW( Color );
}


//--------------------------------------------------------------------------------------
// Set the UVs to be used for the sprite render
//--------------------------------------------------------------------------------------
void Sprite::SetUVs( float fU1, float fV1, float fU2, float fV2 )
{
    D3D11_MAPPED_SUBRESOURCE MappedResource;
    DXUTGetD3D11DeviceContext()->Map( m_pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
    SpriteVertex* pQuadVertices = (SpriteVertex*)MappedResource.pData;

    pQuadVertices[0].v3Pos = DirectX::XMFLOAT3( 0.0f, -1.0f, 0.5f );
    pQuadVertices[0].v2TexCoord = DirectX::XMFLOAT2( fU1, fV1 );
    pQuadVertices[1].v3Pos = DirectX::XMFLOAT3( 0.0f, 0.0f, 0.5f );
    pQuadVertices[1].v2TexCoord = DirectX::XMFLOAT2( fU1, fV2 );
    pQuadVertices[2].v3Pos = DirectX::XMFLOAT3( 1.0f, -1.0f, 0.5f );
    pQuadVertices[2].v2TexCoord = DirectX::XMFLOAT2( fU2, fV1 );
    pQuadVertices[3].v3Pos = DirectX::XMFLOAT3( 0.0f, 0.0f, 0.5f );
    pQuadVertices[3].v2TexCoord = DirectX::XMFLOAT2( fU1, fV2 );
    pQuadVertices[4].v3Pos = DirectX::XMFLOAT3( 1.0f, 0.0f, 0.5f );
    pQuadVertices[4].v2TexCoord = DirectX::XMFLOAT2( fU2, fV2 );
    pQuadVertices[5].v3Pos = DirectX::XMFLOAT3( 1.0f, -1.0f, 0.5f );
    pQuadVertices[5].v2TexCoord = DirectX::XMFLOAT2( fU2, fV1 );

    DXUTGetD3D11DeviceContext()->Unmap( m_pVertexBuffer, 0 );
}


//--------------------------------------------------------------------------------------
// Renders the sprite
//--------------------------------------------------------------------------------------
HRESULT Sprite::RenderSprite( ID3D11ShaderResourceView* pTextureView, int nStartPosX,
    int nStartPosY, int nWidth, int nHeight, bool bAlpha, bool bBordered )
{
    ID3D11ShaderResourceView* pNULLSRV = NULL;

    s_CBSprite.fPlotParams[0] = (float)nStartPosX;
    s_CBSprite.fPlotParams[1] = (float)nStartPosY;
    s_CBSprite.fPlotParams[2] = (float)nWidth;
    s_CBSprite.fPlotParams[3] = (float)nHeight;

    if (pTextureView)
    {
        DXUTGetD3D11DeviceContext()->PSSetShaderResources( 0, 1, &pTextureView );
        DXUTGetD3D11DeviceContext()->PSSetSamplers( 0, 1, m_PointSampleMode ? &m_pSamplePoint : &m_pSampleLinear );
        DXUTGetD3D11DeviceContext()->PSSetShader( m_pSpritePS, NULL, 0 );
    }
    else
    {
        DXUTGetD3D11DeviceContext()->PSSetShader( m_pSpriteUntexturedPS, NULL, 0 );
    }

    float black[4] = { 0, 0, 0, 0 };
    if (bAlpha)
    {
        DXUTGetD3D11DeviceContext()->OMSetBlendState( m_pSrcAlphaBlending, black, 0xFFFFFFFF );
    }
    else
    {
        DXUTGetD3D11DeviceContext()->OMSetBlendState( m_pNoBlending, black, 0xFFFFFFFF );
    }

    // Do the render
    Render();

    DXUTGetD3D11DeviceContext()->PSSetShaderResources( 0, 1, &pNULLSRV );

    // Optionally render a border
    if (bBordered)
    {
        DXUTGetD3D11DeviceContext()->PSSetShader( m_pSpriteBorderPS, NULL, 0 );

        RenderBorder();
    }

    return D3D_OK;
}


//--------------------------------------------------------------------------------------
// Renders the MS sprite
//--------------------------------------------------------------------------------------
HRESULT Sprite::RenderSpriteMS( ID3D11ShaderResourceView* pTextureView, int nStartPosX,
    int nStartPosY, int nWidth, int nHeight, int nTextureWidth, int nTextureHeight,
    bool bAlpha, bool bBordered, int nSampleIndex )
{
    ID3D11SamplerState* ppSamplerStates[2] = { m_pSamplePoint, m_pSampleLinear };
    ID3D11ShaderResourceView* pNULLSRV = NULL;

    assert( NULL != pTextureView );

    s_CBSprite.fPlotParams[0] = (float)nStartPosX;
    s_CBSprite.fPlotParams[1] = (float)nStartPosY;
    s_CBSprite.fPlotParams[2] = (float)nWidth;
    s_CBSprite.fPlotParams[3] = (float)nHeight;
    s_CBSprite.fTextureSize[0] = (float)nTextureWidth;
    s_CBSprite.fTextureSize[1] = (float)nTextureHeight;
    s_CBSprite.fSampleIndex[0] = (float)nSampleIndex;

    DXUTGetD3D11DeviceContext()->PSSetShaderResources( 0, 1, &pTextureView );
    DXUTGetD3D11DeviceContext()->PSSetSamplers( 0, 2, ppSamplerStates );
    DXUTGetD3D11DeviceContext()->PSSetShader( m_pSpriteMSPS, NULL, 0 );

    float black[4] = { 0, 0, 0, 0 };
    if (bAlpha)
    {
        DXUTGetD3D11DeviceContext()->OMSetBlendState( m_pSrcAlphaBlending, black, 0xFFFFFFFF );
    }
    else
    {
        DXUTGetD3D11DeviceContext()->OMSetBlendState( m_pNoBlending, black, 0xFFFFFFFF );
    }

    // Do the render
    Render();

    DXUTGetD3D11DeviceContext()->PSSetShaderResources( 0, 1, &pNULLSRV );

    // Optionally render a border
    if (bBordered)
    {
        DXUTGetD3D11DeviceContext()->PSSetShader( m_pSpriteBorderPS, NULL, 0 );

        RenderBorder();
    }

    return D3D_OK;
}


//--------------------------------------------------------------------------------------
// Renders sprite as depth
//--------------------------------------------------------------------------------------
HRESULT Sprite::RenderSpriteAsDepth( ID3D11ShaderResourceView* pTextureView, int nStartPosX,
    int nStartPosY, int nWidth, int nHeight, bool bBordered, float fDepthRangeMin,
    float fDepthRangeMax )
{
    ID3D11SamplerState* ppSamplerStates[2] = { m_pSamplePoint, m_pSampleLinear };
    ID3D11ShaderResourceView* pNULLSRV = NULL;

    assert( NULL != pTextureView );

    s_CBSprite.fPlotParams[0] = (float)nStartPosX;
    s_CBSprite.fPlotParams[1] = (float)nStartPosY;
    s_CBSprite.fPlotParams[2] = (float)nWidth;
    s_CBSprite.fPlotParams[3] = (float)nHeight;
    s_CBSprite.fDepthRange[0] = fDepthRangeMin;
    s_CBSprite.fDepthRange[1] = fDepthRangeMax;

    DXUTGetD3D11DeviceContext()->PSSetShaderResources( 0, 1, &pTextureView );
    DXUTGetD3D11DeviceContext()->PSSetSamplers( 0, 2, ppSamplerStates );
    DXUTGetD3D11DeviceContext()->PSSetShader( m_pSpriteAsDepthPS, NULL, 0 );

    float black[4] = { 0, 0, 0, 0 };
    DXUTGetD3D11DeviceContext()->OMSetBlendState( m_pNoBlending, black, 0xFFFFFFFF );

    // Do the render
    Render();

    DXUTGetD3D11DeviceContext()->PSSetShaderResources( 0, 1, &pNULLSRV );

    // Optionally render a border
    if (bBordered)
    {
        DXUTGetD3D11DeviceContext()->PSSetShader( m_pSpriteBorderPS, NULL, 0 );
        RenderBorder();
    }

    return D3D_OK;
}


//--------------------------------------------------------------------------------------
// Renders the sprite as MS depth
//--------------------------------------------------------------------------------------
HRESULT Sprite::RenderSpriteAsDepthMS( ID3D11ShaderResourceView* pTextureView, int nStartPosX,
    int nStartPosY, int nWidth, int nHeight, int nTextureWidth, int nTextureHeight,
    bool bBordered, float fDepthRangeMin, float fDepthRangeMax, int nSampleIndex )
{
    ID3D11SamplerState* ppSamplerStates[2] = { m_pSamplePoint, m_pSampleLinear };
    ID3D11ShaderResourceView* pNULLSRV = NULL;

    assert( NULL != pTextureView );

    s_CBSprite.fPlotParams[0] = (float)nStartPosX;
    s_CBSprite.fPlotParams[1] = (float)nStartPosY;
    s_CBSprite.fPlotParams[2] = (float)nWidth;
    s_CBSprite.fPlotParams[3] = (float)nHeight;
    s_CBSprite.fTextureSize[0] = (float)nTextureWidth;
    s_CBSprite.fTextureSize[1] = (float)nTextureHeight;
    s_CBSprite.fSampleIndex[0] = (float)nSampleIndex;
    s_CBSprite.fDepthRange[0] = fDepthRangeMin;
    s_CBSprite.fDepthRange[1] = fDepthRangeMax;

    DXUTGetD3D11DeviceContext()->PSSetShaderResources( 0, 1, &pTextureView );
    DXUTGetD3D11DeviceContext()->PSSetSamplers( 0, 2, ppSamplerStates );
    DXUTGetD3D11DeviceContext()->PSSetShader( m_pSpriteAsDepthMSPS, NULL, 0 );

    float black[4] = { 0, 0, 0, 0 };
    DXUTGetD3D11DeviceContext()->OMSetBlendState( m_pNoBlending, black, 0xFFFFFFFF );

    // Do the render
    Render();

    DXUTGetD3D11DeviceContext()->PSSetShaderResources( 0, 1, &pNULLSRV );
    DXUTGetD3D11DeviceContext()->PSSetShader( m_pSpriteBorderPS, NULL, 0 );

    // Optionally render a border
    if (bBordered)
    {
        RenderBorder();
    }

    return D3D_OK;
}


HRESULT Sprite::RenderSpriteVolume( ID3D11ShaderResourceView* pTextureView, int nStartPosX, int nStartPosY, int nMaxWidth, int nSliceSize, bool bBordered )
{
    ID3D11Texture3D* texture = 0;
    pTextureView->GetResource( (ID3D11Resource**)&texture );
    D3D11_TEXTURE3D_DESC desc;
    texture->GetDesc( &desc );
    texture->Release();

    s_CBSprite.fPlotParams[2] = (float)nSliceSize;
    s_CBSprite.fPlotParams[3] = (float)nSliceSize;

    s_CBSprite.fTextureSize[0] = (float)desc.Width;
    s_CBSprite.fTextureSize[1] = (float)desc.Height;

    DXUTGetD3D11DeviceContext()->PSSetShaderResources( 0, 1, &pTextureView );
    DXUTGetD3D11DeviceContext()->PSSetShader( m_pSpriteVolumePS, NULL, 0 );


    /*  if( bAlpha )
    {
    DXUTGetD3D11DeviceContext()->OMSetBlendState( m_pSrcAlphaBlending, D3DXVECTOR4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
    }
    else*/
    {
        float black[4] = { 0, 0, 0, 0 };
        DXUTGetD3D11DeviceContext()->OMSetBlendState( m_pNoBlending, black, 0xFFFFFFFF );
    }

    // Do the render
    for (int j = 0; j < (bBordered ? 2 : 1); j++)
    {
        if (j == 1)
        {
            DXUTGetD3D11DeviceContext()->PSSetShader( m_pSpriteBorderPS, NULL, 0 );
        }

        float fYPos = (float)nStartPosY;
        float fXPos = 0.0f;
        for (UINT i = 0; i < desc.Depth; i++)
        {
            s_CBSprite.fPlotParams[0] = (float)nStartPosX + fXPos;
            s_CBSprite.fPlotParams[1] = fYPos;
            s_CBSprite.fTextureSize[2] = (float)i;

            if (j == 0)
            {
                Render();
            }
            else
            {
                RenderBorder();
            }

            fXPos += (float)nSliceSize;
            if (fXPos > (float)(nMaxWidth - nSliceSize))
            {
                fXPos = 0.0f;
                fYPos += (float)nSliceSize;
            }
        }
    }

    return D3D_OK;
}


//--------------------------------------------------------------------------------------
// Renders the border
//--------------------------------------------------------------------------------------
void Sprite::RenderBorder()
{
    // Set states
    DXUTGetD3D11DeviceContext()->RSSetState( m_EnableScissorTest ? m_pRasterStateWithScissor : m_pRasterState );
    DXUTGetD3D11DeviceContext()->OMSetDepthStencilState( m_pDisableDepthTestWrite, 0x00 );

    // Update constant buffer
    D3D11_MAPPED_SUBRESOURCE MappedResource;
    DXUTGetD3D11DeviceContext()->Map( m_pcbSprite, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
    memcpy( MappedResource.pData, &s_CBSprite, sizeof( CB_SPRITE ) );
    DXUTGetD3D11DeviceContext()->Unmap( m_pcbSprite, 0 );
    DXUTGetD3D11DeviceContext()->VSSetConstantBuffers( 0, 1, &m_pcbSprite );
    DXUTGetD3D11DeviceContext()->PSSetConstantBuffers( 0, 1, &m_pcbSprite );

    // Set vertex shader
    DXUTGetD3D11DeviceContext()->VSSetShader( m_pSpriteBorderVS, NULL, 0 );
    DXUTGetD3D11DeviceContext()->HSSetShader( NULL, NULL, 0 );
    DXUTGetD3D11DeviceContext()->DSSetShader( NULL, NULL, 0 );
    DXUTGetD3D11DeviceContext()->GSSetShader( NULL, NULL, 0 );

    // Set the input layout
    DXUTGetD3D11DeviceContext()->IASetInputLayout( m_pBorderVertexLayout );

    // Set vertex buffer
    UINT Stride = sizeof( SpriteBorderVertex );
    UINT Offset = 0;
    DXUTGetD3D11DeviceContext()->IASetVertexBuffers( 0, 1, &m_pBorderVertexBuffer, &Stride, &Offset );

    // Set primitive topology
    DXUTGetD3D11DeviceContext()->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP );

    // Render
    DXUTGetD3D11DeviceContext()->Draw( 5, 0 );

    // Reset states
    DXUTGetD3D11DeviceContext()->RSSetState( m_pEnableCulling );
    DXUTGetD3D11DeviceContext()->OMSetDepthStencilState( m_pEnableDepthTestWrite, 0xff );
}


//--------------------------------------------------------------------------------------
// Actually does the plot
//--------------------------------------------------------------------------------------
void Sprite::Render()
{
    // Set states
    DXUTGetD3D11DeviceContext()->RSSetState( m_EnableScissorTest ? m_pRasterStateWithScissor : m_pRasterState );
    DXUTGetD3D11DeviceContext()->OMSetDepthStencilState( m_pDisableDepthTestWrite, 0x00 );

    // Update constant buffer
    D3D11_MAPPED_SUBRESOURCE MappedResource;
    DXUTGetD3D11DeviceContext()->Map( m_pcbSprite, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
    memcpy( MappedResource.pData, &s_CBSprite, sizeof( CB_SPRITE ) );
    DXUTGetD3D11DeviceContext()->Unmap( m_pcbSprite, 0 );
    DXUTGetD3D11DeviceContext()->VSSetConstantBuffers( 0, 1, &m_pcbSprite );
    DXUTGetD3D11DeviceContext()->PSSetConstantBuffers( 0, 1, &m_pcbSprite );

    // Set vertex shader
    DXUTGetD3D11DeviceContext()->VSSetShader( m_pSpriteVS, NULL, 0 );
    DXUTGetD3D11DeviceContext()->HSSetShader( NULL, NULL, 0 );
    DXUTGetD3D11DeviceContext()->DSSetShader( NULL, NULL, 0 );
    DXUTGetD3D11DeviceContext()->GSSetShader( NULL, NULL, 0 );

    // Set the input layout
    DXUTGetD3D11DeviceContext()->IASetInputLayout( m_pVertexLayout );

    // Set vertex buffer
    UINT Stride = sizeof( SpriteVertex );
    UINT Offset = 0;
    DXUTGetD3D11DeviceContext()->IASetVertexBuffers( 0, 1, &m_pVertexBuffer, &Stride, &Offset );

    // Set primitive topology
    DXUTGetD3D11DeviceContext()->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

    // Render
    DXUTGetD3D11DeviceContext()->Draw( 6, 0 );

    // Reset states
    DXUTGetD3D11DeviceContext()->RSSetState( m_pEnableCulling );
    DXUTGetD3D11DeviceContext()->OMSetDepthStencilState( m_pEnableDepthTestWrite, 0xff );
}
