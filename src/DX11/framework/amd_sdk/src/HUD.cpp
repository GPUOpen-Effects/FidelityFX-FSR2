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
// File: HUD.cpp
//
// Class definition for the AMD standard HUD interface.
//--------------------------------------------------------------------------------------

// DXUT helper code
#include "DXUT.h"
#include "DXUTmisc.h"
#include "DXUTgui.h"
#include "SDKmisc.h"
#include "DDSTextureLoader.h"

#include "HUD.h"

using namespace AMD;

//--------------------------------------------------------------------------------------
// Constructor
//--------------------------------------------------------------------------------------
HUD::HUD()
{
    m_pLogoSRV = NULL;
}


//--------------------------------------------------------------------------------------
// Destructor
//--------------------------------------------------------------------------------------
HUD::~HUD()
{
    SAFE_RELEASE( m_pLogoSRV );
}


//--------------------------------------------------------------------------------------
// Device creation hook function, that loads the AMD logo texture, and creates a sprite
// object
//--------------------------------------------------------------------------------------
HRESULT HUD::OnCreateDevice( ID3D11Device* pd3dDevice )
{
    HRESULT hr;
    wchar_t str[MAX_PATH];

    m_Sprite.OnCreateDevice( pd3dDevice );

    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"dx11\\UI\\AMD_FidelityFX.dds" ) );
    DirectX::CreateDDSTextureFromFile( pd3dDevice, str, nullptr, &m_pLogoSRV );
    DXUT_SetDebugName( m_pLogoSRV, "AMD_FidelityFX.dds" );

    return hr;
}


//--------------------------------------------------------------------------------------
// Device destruction hook function, that releases the sprite object and
//--------------------------------------------------------------------------------------
void HUD::OnDestroyDevice()
{
    m_Sprite.OnDestroyDevice();

    SAFE_RELEASE( m_pLogoSRV );
}


//--------------------------------------------------------------------------------------
// Resize swap chain hook function, that passes through to the sprite object
//--------------------------------------------------------------------------------------
void HUD::OnResizedSwapChain( const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc )
{
    m_Sprite.OnResizedSwapChain( pBackBufferSurfaceDesc );
}


//--------------------------------------------------------------------------------------
// Render hook function, that calls the CDXUTDialog::OnRender method, and additionally
// renders the AMD sprite
//--------------------------------------------------------------------------------------
void HUD::OnRender( float fElapsedTime )
{
    m_GUI.OnRender( fElapsedTime );
    m_Sprite.RenderSprite( m_pLogoSRV, DXUTGetDXGIBackBufferSurfaceDesc()->Width - 220, DXUTGetDXGIBackBufferSurfaceDesc()->Height - 2, 200, 75, true, false );
}


Slider::Slider( CDXUTDialog& dialog, int id, int& y, const wchar_t* label, int min, int max, int& value ) :
m_Value( value ),
m_szLabel( label ),
m_ValueFloat( (float &)value )
{
    dialog.AddStatic( id + 1000000, L"", AMD::HUD::iElementOffset, y += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, false, &m_pLabel );
    dialog.AddSlider( id, AMD::HUD::iElementOffset, y += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, min, max, m_Value, false, &m_pSlider );

    dialog.AddControl( this );

    m_Min = min;
    m_Max = max;

    m_UseFloat = false;
    OnGuiEvent();
}


Slider::Slider( CDXUTDialog& dialog, int id, int& y, const wchar_t* label, float min, float max, float step, float& value ) :
m_ValueFloat( value ),
m_MinFloat( min ),
m_MaxFloat( max ),
m_szLabel( label ),
m_Value( (int &)value )
{
    m_Min = 0;
    m_Max = (int)((m_MaxFloat - m_MinFloat) / step + 0.499f);

    int slider_value = (int)((m_ValueFloat - m_MinFloat) / (m_MaxFloat - m_MinFloat) * m_Max + 0.499f);
    dialog.AddStatic( id + 1000000, L"", AMD::HUD::iElementOffset, y += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, false, &m_pLabel );
    dialog.AddSlider( id, AMD::HUD::iElementOffset, y += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, m_Min, m_Max, slider_value, false, &m_pSlider );

    dialog.AddControl( this );

    m_UseFloat = true;
    OnGuiEvent();
}


void Slider::OnGuiEvent()
{
    if (m_UseFloat)
    {
        m_ValueFloat = (m_MaxFloat - m_MinFloat) * ((float)m_pSlider->GetValue() / m_Max) + m_MinFloat;
    }
    else
    {
        m_Value = m_pSlider->GetValue();
    }

    wchar_t buff[1024];
    if (m_UseFloat)
    {
        swprintf_s( buff, 1024, L"%s: %f", m_szLabel, m_ValueFloat );
    }
    else
    {
        swprintf_s( buff, 1024, L"%s: %d", m_szLabel, m_Value );
    }

    m_pLabel->SetText( buff );
}


void Slider::SetEnabled( bool enable )
{
    m_pLabel->SetEnabled( enable );
    m_pSlider->SetEnabled( enable );
}


void Slider::SetVisible( bool visible )
{
    m_pLabel->SetVisible( visible );
    m_pSlider->SetVisible( visible );
}


void Slider::SetValue( int value )
{
    if (m_UseFloat == true) { return; }
    m_pSlider->SetValue( value );
    OnGuiEvent();
}

void Slider::SetValue( float value )
{
    if (m_UseFloat != true) { return; }

    int slider_value = (int)((value - m_MinFloat) / (m_MaxFloat - m_MinFloat) * m_Max + 0.499f);

    m_pSlider->SetValue( (int)slider_value );
    OnGuiEvent();
}
