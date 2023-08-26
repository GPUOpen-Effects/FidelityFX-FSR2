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
// File: Magnify.h
//
// Magnify class definition. This class magnifies a region of a given surface
// and renders a scaled sprite at the given position on the screen.
//--------------------------------------------------------------------------------------
#ifndef AMD_SDK_MAGNIFY_H
#define AMD_SDK_MAGNIFY_H

#include "Sprite.h"

namespace AMD
{

class Magnify
{
public:

    // Constructor / destructor
    Magnify();
    ~Magnify();

    // Hooks for the DX SDK Framework
    HRESULT OnCreateDevice( ID3D11Device* pd3dDevice );
    void OnDestroyDevice();
    void OnResizedSwapChain( const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc );

    // Set methods
    void SetPixelRegion( int nPixelRegion );
    void SetScale( int nScale );
    void SetDepthRangeMin( float fDepthRangeMin );
    void SetDepthRangeMax( float fDepthRangeMax );
    void SetSourceResource( ID3D11Resource* pSourceResource, DXGI_FORMAT Format,
        int nWidth, int nHeight, int nSamples );
    void SetSubSampleIndex( int nSubSampleIndex );

    // Captures a region, at the current cursor position, for magnification
    void Capture( POINT& Point );

    // Render the magnified region, at the capture location
    void RenderBackground();
    void RenderMagnifiedRegion();

private:

    // Private methods
    void SetPosition( int nPositionX, int nPositionY );
    void CreateInternalResources();

private:

    // Magnification settings
    int     m_nPositionX;
    int     m_nPositionY;
    int     m_nPixelRegion;
    int     m_nHalfPixelRegion;
    int     m_nScale;
    float   m_fDepthRangeMin;
    float   m_fDepthRangeMax;
    int     m_nBackBufferWidth;
    int     m_nBackBufferHeight;
    int     m_nSubSampleIndex;

    // Helper class for plotting the magnified region
    Sprite  m_Sprite;

    // Source resource data
    ID3D11Resource*             m_pSourceResource;
    ID3D11Texture2D*            m_pResolvedSourceResource;
    ID3D11Texture2D*            m_pCopySourceResource;
    ID3D11ShaderResourceView*   m_pResolvedSourceResourceSRV;
    ID3D11ShaderResourceView*   m_pCopySourceResourceSRV;
    ID3D11ShaderResourceView*   m_pSourceResourceSRV1;
    DXGI_FORMAT                 m_SourceResourceFormat;
    int                         m_nSourceResourceWidth;
    int                         m_nSourceResourceHeight;
    int                         m_nSourceResourceSamples;
    DXGI_FORMAT                 m_DepthFormat;
    DXGI_FORMAT                 m_DepthSRVFormat;
    bool                        m_bDepthFormat;
};

} // namespace AMD

#endif // AMD_SDK_MAGNIFY_H
