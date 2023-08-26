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
// File: Magnify.cpp
//
// Magnify class implementation. This class magnifies a region of a given surface, and
// renders a scaled sprite at the given position on the screen.
//--------------------------------------------------------------------------------------


#include "DXUT.h"
#include "Magnify.h"

using namespace AMD;

//--------------------------------------------------------------------------------------
// Constructor
//--------------------------------------------------------------------------------------
Magnify::Magnify()
{
    // Magnification settings
    m_nPixelRegion = 64;
    m_nHalfPixelRegion = m_nPixelRegion / 2;
    m_nScale = 5;
    m_nPositionX = m_nPixelRegion;
    m_nPositionY = m_nPixelRegion;
    m_fDepthRangeMin = 0.0f;
    m_fDepthRangeMax = 0.01f;
    m_nBackBufferWidth = 0;
    m_nBackBufferHeight = 0;
    m_nSubSampleIndex = 0;

    // Source resource data
    m_pSourceResource = NULL;
    m_pResolvedSourceResource = NULL;
    m_pCopySourceResource = NULL;
    m_pResolvedSourceResourceSRV = NULL;
    m_pCopySourceResourceSRV = NULL;
    m_pSourceResourceSRV1 = NULL;
    m_SourceResourceFormat = DXGI_FORMAT_UNKNOWN;
    m_nSourceResourceWidth = 0;
    m_nSourceResourceHeight = 0;
    m_nSourceResourceSamples = 0;
    m_DepthFormat = DXGI_FORMAT_UNKNOWN;
    m_DepthSRVFormat = DXGI_FORMAT_UNKNOWN;
    m_bDepthFormat = false;
}


//--------------------------------------------------------------------------------------
// Destructor
//--------------------------------------------------------------------------------------
Magnify::~Magnify()
{
}


//--------------------------------------------------------------------------------------
// Hook function
//--------------------------------------------------------------------------------------
HRESULT Magnify::OnCreateDevice( ID3D11Device* pd3dDevice )
{
    HRESULT hr;

    assert( NULL != pd3dDevice );

    hr = m_Sprite.OnCreateDevice( pd3dDevice );
    assert( S_OK == hr );

    return hr;
}


//--------------------------------------------------------------------------------------
// Hook function
//--------------------------------------------------------------------------------------
void Magnify::OnDestroyDevice()
{
    m_Sprite.OnDestroyDevice();

    SAFE_RELEASE( m_pResolvedSourceResourceSRV );
    SAFE_RELEASE( m_pCopySourceResourceSRV );
    SAFE_RELEASE( m_pSourceResourceSRV1 );
    SAFE_RELEASE( m_pResolvedSourceResource );
    SAFE_RELEASE( m_pCopySourceResource );
}


//--------------------------------------------------------------------------------------
// Hook function
//--------------------------------------------------------------------------------------
void Magnify::OnResizedSwapChain( const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc )
{
    assert( NULL != pBackBufferSurfaceDesc );

    m_Sprite.OnResizedSwapChain( pBackBufferSurfaceDesc );

    m_nBackBufferWidth = pBackBufferSurfaceDesc->Width;
    m_nBackBufferHeight = pBackBufferSurfaceDesc->Height;
}


//--------------------------------------------------------------------------------------
// Sets the region to be captured from the source resource
//--------------------------------------------------------------------------------------
void Magnify::Capture( POINT& Point )
{
    RECT Rect;
    ::GetWindowRect( DXUTGetHWND(), &Rect );

    int nWidthDiff = 0;
    int nHeightDiff = 0;

    if (DXUTIsWindowed())
    {
        nWidthDiff = (int)(((Rect.right - Rect.left) - m_nBackBufferWidth) * (1.0f / 2.0f));
        nHeightDiff = (int)(((Rect.bottom - Rect.top) - m_nBackBufferHeight) * (4.0f / 5.0f));
    }

    SetPosition( Point.x - (Rect.left + nWidthDiff), Point.y - (Rect.top + nHeightDiff) );

    D3D10_BOX SourceRegion;
    SourceRegion.left = m_nPositionX - m_nHalfPixelRegion;
    SourceRegion.right = m_nPositionX + m_nHalfPixelRegion;
    SourceRegion.top = m_nPositionY - m_nHalfPixelRegion;
    SourceRegion.bottom = m_nPositionY + m_nHalfPixelRegion;
    SourceRegion.front = 0;
    SourceRegion.back = 1;
}


//--------------------------------------------------------------------------------------
// User defines the resource for capturing from
//--------------------------------------------------------------------------------------
void Magnify::SetSourceResource( ID3D11Resource* pSourceResource, DXGI_FORMAT Format,
    int nWidth, int nHeight, int nSamples )
{
    assert( NULL != pSourceResource );
    assert( Format > DXGI_FORMAT_UNKNOWN );
    assert( nWidth > 0 );
    assert( nHeight > 0 );
    assert( nSamples > 0 );

    m_pSourceResource = pSourceResource;
    m_SourceResourceFormat = Format;
    m_nSourceResourceWidth = nWidth;
    m_nSourceResourceHeight = nHeight;
    m_nSourceResourceSamples = nSamples;

    m_bDepthFormat = false;

    switch (m_SourceResourceFormat)
    {
    case DXGI_FORMAT_D32_FLOAT:
        m_DepthFormat = DXGI_FORMAT_R32_TYPELESS;
        m_DepthSRVFormat = DXGI_FORMAT_R32_FLOAT;
        m_bDepthFormat = true;
        break;

    case DXGI_FORMAT_D24_UNORM_S8_UINT:
        m_DepthFormat = DXGI_FORMAT_R24G8_TYPELESS;
        m_DepthSRVFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        m_bDepthFormat = true;
        break;

    case DXGI_FORMAT_D16_UNORM:
        m_DepthFormat = DXGI_FORMAT_R16_TYPELESS;
        m_DepthSRVFormat = DXGI_FORMAT_R16_UNORM;
        m_bDepthFormat = true;
        break;
    }

    CreateInternalResources();
}


//--------------------------------------------------------------------------------------
// User defines the pixel region to be captured
//--------------------------------------------------------------------------------------
void Magnify::SetPixelRegion( int nPixelRegion )
{
    assert( nPixelRegion > 1 );

    m_nPixelRegion = nPixelRegion;
    m_nHalfPixelRegion = m_nPixelRegion / 2;
}


//--------------------------------------------------------------------------------------
// User defines the scale of magnification
//--------------------------------------------------------------------------------------
void Magnify::SetScale( int nScale )
{
    assert( nScale > 1 );

    m_nScale = nScale;
}


//--------------------------------------------------------------------------------------
// User defines how to scale depth surfaces
//--------------------------------------------------------------------------------------
void Magnify::SetDepthRangeMin( float fDepthRangeMin )
{
    m_fDepthRangeMin = fDepthRangeMin;
}


//--------------------------------------------------------------------------------------
// User defines how to scale depth surfaces
//--------------------------------------------------------------------------------------
void Magnify::SetDepthRangeMax( float fDepthRangeMax )
{
    m_fDepthRangeMax = fDepthRangeMax;
}


//--------------------------------------------------------------------------------------
// User defines which sub-samnple to magnify for MSAA sufaces
//--------------------------------------------------------------------------------------
void Magnify::SetSubSampleIndex( int nSubSampleIndex )
{
    m_nSubSampleIndex = nSubSampleIndex;
}


//--------------------------------------------------------------------------------------
// Renders the source resource to the screen
//--------------------------------------------------------------------------------------
void Magnify::RenderBackground()
{
    if (m_bDepthFormat)
    {
        if (m_nSourceResourceSamples == 1)
        {
            DXUTGetD3D11DeviceContext()->CopyResource( m_pCopySourceResource, m_pSourceResource );
        }
    }
    else
    {
        if (m_nSourceResourceSamples > 1)
        {
            DXUTGetD3D11DeviceContext()->ResolveSubresource( m_pResolvedSourceResource, 0,
                m_pSourceResource, 0, m_SourceResourceFormat );
        }
        else
        {
            DXUTGetD3D11DeviceContext()->CopyResource( m_pCopySourceResource, m_pSourceResource );

        }
    }

    if (m_bDepthFormat)
    {
        if (m_nSourceResourceSamples > 1)
        {
            // Get the current render and depth targets, so we can later revert to these.
            ID3D11RenderTargetView *pRenderTargetView;
            ID3D11DepthStencilView *pDepthStencilView;
            DXUTGetD3D11DeviceContext()->OMGetRenderTargets( 1, &pRenderTargetView, &pDepthStencilView );

            // Bind our render target view to the OM stage.
            DXUTGetD3D11DeviceContext()->OMSetRenderTargets( 1, (ID3D11RenderTargetView* const*)&pRenderTargetView, NULL );

            m_Sprite.SetUVs( 0.0f, 0.0f, 1.0f, 1.0f );

            m_Sprite.RenderSpriteAsDepthMS( m_pSourceResourceSRV1, 0, m_nBackBufferHeight,
                m_nBackBufferWidth, m_nBackBufferHeight, m_nBackBufferWidth, m_nBackBufferHeight,
                false, m_fDepthRangeMin, m_fDepthRangeMax, m_nSubSampleIndex );

            // Bind back to the original render, depth target, and viewport
            DXUTGetD3D11DeviceContext()->OMSetRenderTargets( 1, (ID3D11RenderTargetView* const*)&pRenderTargetView, pDepthStencilView );

            // Decrement the counter on these resources
            SAFE_RELEASE( pRenderTargetView );
            SAFE_RELEASE( pDepthStencilView );
        }
        else
        {
            m_Sprite.SetUVs( 0.0f, 0.0f, 1.0f, 1.0f );

            m_Sprite.RenderSpriteAsDepth( m_pCopySourceResourceSRV, 0, m_nBackBufferHeight,
                m_nBackBufferWidth, m_nBackBufferHeight, false, m_fDepthRangeMin, m_fDepthRangeMax );
        }
    }
    else
    {
        m_Sprite.SetUVs( 0.0f, 0.0f, 1.0f, 1.0f );

        if (m_nSourceResourceSamples > 1)
        {
            m_Sprite.RenderSprite( m_pResolvedSourceResourceSRV, 0, m_nBackBufferHeight,
                m_nBackBufferWidth, m_nBackBufferHeight, false, false );
        }
        else
        {
            m_Sprite.RenderSprite( m_pCopySourceResourceSRV, 0, m_nBackBufferHeight,
                m_nBackBufferWidth, m_nBackBufferHeight, false, false );
        }
    }
}


//--------------------------------------------------------------------------------------
// Renders the magnified region to the screen
//--------------------------------------------------------------------------------------
void Magnify::RenderMagnifiedRegion()
{
    m_Sprite.SetUVs( (m_nPositionX - m_nHalfPixelRegion) / (float)m_nSourceResourceWidth,
        (m_nPositionY - m_nHalfPixelRegion) / (float)m_nSourceResourceHeight,
        (m_nPositionX + m_nHalfPixelRegion) / (float)m_nSourceResourceWidth,
        (m_nPositionY + m_nHalfPixelRegion) / (float)m_nSourceResourceHeight );

    if (m_bDepthFormat)
    {
        DirectX::XMVECTOR Color = DirectX::XMVectorSet( 1.0f, 1.0f, 1.0f, 1.0f );
        m_Sprite.SetBorderColor( Color );

        if (m_nSourceResourceSamples > 1)
        {
            // Get the current render and depth targets, so we can later revert to these
            ID3D11RenderTargetView *pRenderTargetView;
            ID3D11DepthStencilView *pDepthStencilView;
            DXUTGetD3D11DeviceContext()->OMGetRenderTargets( 1, &pRenderTargetView, &pDepthStencilView );

            // Bind our render target view to the OM stage
            DXUTGetD3D11DeviceContext()->OMSetRenderTargets( 1, (ID3D11RenderTargetView* const*)&pRenderTargetView, NULL );

            m_Sprite.RenderSpriteAsDepthMS( m_pSourceResourceSRV1, (m_nPositionX - m_nHalfPixelRegion * m_nScale),
                (m_nPositionY + m_nHalfPixelRegion * m_nScale), (m_nPixelRegion * m_nScale),
                (m_nPixelRegion * m_nScale), m_nSourceResourceWidth, m_nSourceResourceHeight, true,
                m_fDepthRangeMin, m_fDepthRangeMax, m_nSubSampleIndex );

            // Bind back to the original render, depth target, and viewport
            DXUTGetD3D11DeviceContext()->OMSetRenderTargets( 1, (ID3D11RenderTargetView* const*)&pRenderTargetView, pDepthStencilView );

            // Decrement the counter on these resources
            SAFE_RELEASE( pRenderTargetView );
            SAFE_RELEASE( pDepthStencilView );
        }
        else
        {
            m_Sprite.RenderSpriteAsDepth( m_pCopySourceResourceSRV, (m_nPositionX - m_nHalfPixelRegion * m_nScale),
                (m_nPositionY + m_nHalfPixelRegion * m_nScale), (m_nPixelRegion * m_nScale),
                (m_nPixelRegion * m_nScale), true, m_fDepthRangeMin, m_fDepthRangeMax );
        }
    }
    else
    {
        DirectX::XMVECTOR Color = DirectX::XMVectorSet( 1.0f, 0.0f, 0.0f, 0.0f );
        m_Sprite.SetBorderColor( Color );

        if (m_nSourceResourceSamples > 1)
        {
            m_Sprite.RenderSprite( m_pResolvedSourceResourceSRV, (m_nPositionX - m_nHalfPixelRegion * m_nScale),
                (m_nPositionY + m_nHalfPixelRegion * m_nScale), (m_nPixelRegion * m_nScale),
                (m_nPixelRegion * m_nScale), false, true );
        }
        else
        {
            m_Sprite.RenderSprite( m_pCopySourceResourceSRV, (m_nPositionX - m_nHalfPixelRegion * m_nScale),
                (m_nPositionY + m_nHalfPixelRegion * m_nScale), (m_nPixelRegion * m_nScale),
                (m_nPixelRegion * m_nScale), false, true );
        }
    }
}


//--------------------------------------------------------------------------------------
// Sets the position of capture
//--------------------------------------------------------------------------------------
void Magnify::SetPosition( int nPositionX, int nPositionY )
{
    m_nPositionX = nPositionX;
    m_nPositionY = nPositionY;

    int nMinX = m_nPixelRegion;
    int nMaxX = m_nSourceResourceWidth - m_nPixelRegion;
    int nMinY = m_nPixelRegion;
    int nMaxY = m_nSourceResourceHeight - m_nPixelRegion;

    m_nPositionX = (m_nPositionX < nMinX) ? (nMinX) : (m_nPositionX);
    m_nPositionX = (m_nPositionX > nMaxX) ? (nMaxX) : (m_nPositionX);

    m_nPositionY = (m_nPositionY < nMinY) ? (nMinY) : (m_nPositionY);
    m_nPositionY = (m_nPositionY > nMaxY) ? (nMaxY) : (m_nPositionY);
}


//--------------------------------------------------------------------------------------
// Private method that creates various interanl resources
//--------------------------------------------------------------------------------------
void Magnify::CreateInternalResources()
{
    HRESULT hr;

    SAFE_RELEASE( m_pResolvedSourceResourceSRV );
    SAFE_RELEASE( m_pCopySourceResourceSRV );
    SAFE_RELEASE( m_pSourceResourceSRV1 );
    SAFE_RELEASE( m_pResolvedSourceResource );
    SAFE_RELEASE( m_pCopySourceResource );

    D3D11_TEXTURE2D_DESC Desc;
    ZeroMemory( &Desc, sizeof( Desc ) );
    Desc.Width = m_nBackBufferWidth;
    Desc.Height = m_nBackBufferHeight;
    Desc.MipLevels = 1;
    Desc.ArraySize = 1;
    Desc.Format = (m_bDepthFormat) ? (m_DepthFormat) : (m_SourceResourceFormat);
    Desc.SampleDesc.Count = 1;
    Desc.Usage = D3D11_USAGE_DEFAULT;
    Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    hr = DXUTGetD3D11Device()->CreateTexture2D( &Desc, NULL, &m_pResolvedSourceResource );
    assert( S_OK == hr );

    Desc.SampleDesc.Count = m_nSourceResourceSamples;

    hr = DXUTGetD3D11Device()->CreateTexture2D( &Desc, NULL, &m_pCopySourceResource );
    assert( S_OK == hr );

    if (m_bDepthFormat)
    {
        if (m_nSourceResourceSamples > 1)
        {
            if (DXUTGetD3D11DeviceFeatureLevel() >= D3D_FEATURE_LEVEL_10_1)
            {
                D3D11_SHADER_RESOURCE_VIEW_DESC SRDesc;
                SRDesc.Format = m_DepthSRVFormat;
                SRDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;

                hr = DXUTGetD3D11Device()->CreateShaderResourceView( m_pSourceResource, &SRDesc, &m_pSourceResourceSRV1 );
                assert( S_OK == hr );
            }
        }
        else
        {
            D3D11_SHADER_RESOURCE_VIEW_DESC SRDesc;
            SRDesc.Format = m_DepthSRVFormat;
            SRDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            SRDesc.Texture2D.MostDetailedMip = 0;
            SRDesc.Texture2D.MipLevels = 1;

            hr = DXUTGetD3D11Device()->CreateShaderResourceView( m_pCopySourceResource, &SRDesc, &m_pCopySourceResourceSRV );
            assert( S_OK == hr );
        }
    }
    else
    {
        if (m_nSourceResourceSamples > 1)
        {
            hr = DXUTGetD3D11Device()->CreateShaderResourceView( m_pResolvedSourceResource, NULL, &m_pResolvedSourceResourceSRV );
            assert( S_OK == hr );
        }
        else
        {
            hr = DXUTGetD3D11Device()->CreateShaderResourceView( m_pCopySourceResource, NULL, &m_pCopySourceResourceSRV );
            assert( S_OK == hr );
        }
    }
}
