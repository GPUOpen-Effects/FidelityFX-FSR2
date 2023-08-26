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
// File: HUD.h
//
// Class definition for the AMD standard HUD interface.
//--------------------------------------------------------------------------------------
#ifndef AMD_SDK_HUD_H
#define AMD_SDK_HUD_H

#include "Sprite.h"

namespace AMD
{

class HUD
{
public:

    // AMD standard HUD defines for GUI spacing
    static const int iElementDelta = 25;
    static const int iGroupDelta = ( iElementDelta * 2 );
    static const int iDialogWidth = 250;
    static const int iElementHeight = 24;
    static const int iElementWidth = 170;
    static const int iElementOffset = ( iDialogWidth - iElementWidth ) / 2;
    static const int iElementDropHeight = 35;

    // Public access to the CDXUTDialog is allowed for ease of use in the sample
    CDXUTDialog m_GUI;

    // Constructor / destructor
    HUD();
    ~HUD();

    // Various hook functions
    HRESULT OnCreateDevice( ID3D11Device* pd3dDevice );
    void OnDestroyDevice();
    void OnResizedSwapChain( const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc );
    void OnRender( float fElapsedTime );

private:

    // The private AMD logo texture, and sprite object
    Sprite                      m_Sprite;
    ID3D11ShaderResourceView*   m_pLogoSRV;
};


class Slider : public CDXUTControl
{
public:

    Slider( CDXUTDialog& dialog, int id, int& y, const wchar_t* label, int min, int max, int& value );
    Slider( CDXUTDialog& dialog, int id, int& y, const wchar_t* label, float min, float max, float step, float& value );
    virtual ~Slider() {}

    void OnGuiEvent();
    void SetEnabled( bool enable );
    void SetVisible( bool visible );
    void SetValue( int value );
    void SetValue( float value );

private:

    Slider& operator=( const Slider& );

    int             m_Min, m_Max;
    float           m_MinFloat, m_MaxFloat;

    bool            m_UseFloat;
    int&            m_Value;
    float&          m_ValueFloat;

    const wchar_t*  m_szLabel;
    CDXUTSlider*    m_pSlider;
    CDXUTStatic*    m_pLabel;
};

} // namespace AMD

#endif
