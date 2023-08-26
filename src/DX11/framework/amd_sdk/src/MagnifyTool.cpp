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
// File: MagnifyTool.cpp
//
// MagnifyTool class implementation. This class implements a user interface based upon the
// DXUT framework, for the Magnify class.
//--------------------------------------------------------------------------------------

// DXUT helper code
#include "DXUT.h"
#include "DXUTgui.h"

#include "MagnifyTool.h"
#include "HUD.h"

#pragma warning( disable : 4100 ) // disable unreference formal parameter warnings for /W4 builds

using namespace AMD;


//--------------------------------------------------------------------------------------
// Constructor
//--------------------------------------------------------------------------------------
MagnifyTool::MagnifyTool()
{
    m_pSourceRTResource = NULL;
    m_bReleaseRTOnResize = false;
    m_RTFormat = DXGI_FORMAT_UNKNOWN;
    m_nWidth = 0;
    m_nHeight = 0;
    m_nSamples = 0;
    m_pMagnifyUI = NULL;
    m_bMouseDownLastFrame = false;
    m_bStickyShowing = false;
}


//--------------------------------------------------------------------------------------
// Destructor
//--------------------------------------------------------------------------------------
MagnifyTool::~MagnifyTool()
{
}


//--------------------------------------------------------------------------------------
// User sets the resource to be captured from
//--------------------------------------------------------------------------------------
void MagnifyTool::SetSourceResources( ID3D11Resource* pSourceRTResource, DXGI_FORMAT RTFormat,
    int nWidth, int nHeight, int nSamples )
{
    assert( NULL != pSourceRTResource );

    m_pSourceRTResource = pSourceRTResource;
    m_RTFormat = RTFormat;
    m_nWidth = nWidth;
    m_nHeight = nHeight;
    m_nSamples = nSamples;

    if (NULL != pSourceRTResource)
    {
        m_Magnify.SetSourceResource( m_pSourceRTResource, m_RTFormat, m_nWidth, m_nHeight, m_nSamples );
    }

    EnableTool( true );
    EnableUI( true );
}


//--------------------------------------------------------------------------------------
// Hook function
//--------------------------------------------------------------------------------------
void MagnifyTool::InitApp( CDXUTDialog* pUI, int& iStartHeight, bool bSupportStickyMode )
{
    m_pMagnifyUI = pUI;

    int& iY = iStartHeight;

    m_pMagnifyUI->AddCheckBox( IDC_MAGNIFY_CHECKBOX_ENABLE, L"Magnify: RMouse", AMD::HUD::iElementOffset, iY, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, true );
    if (bSupportStickyMode)
    {
        m_pMagnifyUI->AddCheckBox( IDC_MAGNIFY_CHECKBOX_STICKY, L"Sticky Mode", AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, true );
    }

    m_pMagnifyUI->AddStatic( IDC_MAGNIFY_STATIC_PIXEL_REGION, L"Magnify Region : 128", AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight );
    m_pMagnifyUI->AddSlider( IDC_MAGNIFY_SLIDER_PIXEL_REGION, AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, 16, 256, 128, true );

    m_pMagnifyUI->AddStatic( IDC_MAGNIFY_STATIC_SCALE, L"Magnify Scale : 5", AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight );
    m_pMagnifyUI->AddSlider( IDC_MAGNIFY_SLIDER_SCALE, AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, 2, 10, 5, true );

    EnableTool( false );
    EnableUI( false );
}


//--------------------------------------------------------------------------------------
// Hook function
//--------------------------------------------------------------------------------------
HRESULT MagnifyTool::OnCreateDevice( ID3D11Device* pd3dDevice )
{
    HRESULT hr;

    assert( NULL != pd3dDevice );

    hr = m_Magnify.OnCreateDevice( pd3dDevice );
    assert( S_OK == hr );

    return hr;
}


//--------------------------------------------------------------------------------------
// Hook function
//--------------------------------------------------------------------------------------
void MagnifyTool::OnResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain *pSwapChain,
    const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext,
    int nPositionX, int nPositionY )
{
    m_pMagnifyUI->SetLocation( nPositionX, nPositionY );
    m_pMagnifyUI->SetSize( AMD::HUD::iDialogWidth, pBackBufferSurfaceDesc->Height );

    m_Magnify.OnResizedSwapChain( DXUTGetDXGIBackBufferSurfaceDesc() );
}


//--------------------------------------------------------------------------------------
// Hook function
//--------------------------------------------------------------------------------------
void MagnifyTool::Render()
{
    if (m_pMagnifyUI->GetCheckBox( IDC_MAGNIFY_CHECKBOX_ENABLE )->GetEnabled() &&
        m_pMagnifyUI->GetCheckBox( IDC_MAGNIFY_CHECKBOX_ENABLE )->GetChecked())
    {
        bool bRenderMagnifier = false;
        POINT pt;

        m_Magnify.RenderBackground();

        if (DXUTIsMouseButtonDown( VK_RBUTTON ))
        {
            if (m_pMagnifyUI->GetCheckBox( IDC_MAGNIFY_CHECKBOX_STICKY ) && m_pMagnifyUI->GetCheckBox( IDC_MAGNIFY_CHECKBOX_STICKY )->GetChecked())
            {
                if (!m_bMouseDownLastFrame)
                {
                    m_bStickyShowing ^= 1;
                }
            }
            else
            {
                bRenderMagnifier = true;
            }

            ::GetCursorPos( &pt );

            if (m_bStickyShowing)
            {
                m_StickyPoint = pt;
            }

            if (!m_bMouseDownLastFrame)
            {
                m_bMouseDownLastFrame = true;
            }
        }
        else
        {
            if (m_bMouseDownLastFrame)
            {
                m_bMouseDownLastFrame = false;
            }
        }

        if (m_bStickyShowing)
        {
            bRenderMagnifier = true;
            pt = m_StickyPoint;
        }

        if (bRenderMagnifier)
        {
            m_Magnify.Capture( pt );
            m_Magnify.RenderMagnifiedRegion();
        }
    }
}


//--------------------------------------------------------------------------------------
// Hook function
//--------------------------------------------------------------------------------------
void MagnifyTool::OnDestroyDevice()
{
    m_Magnify.OnDestroyDevice();
}


//--------------------------------------------------------------------------------------
// Hook function
//--------------------------------------------------------------------------------------
void MagnifyTool::OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
{
    WCHAR szTemp[256];
    int nTemp;
    bool bChecked;

    switch (nControlID)
    {
    case IDC_MAGNIFY_CHECKBOX_ENABLE:
        bChecked = ((CDXUTCheckBox*)pControl)->GetChecked();
        EnableUI( bChecked );
        break;

    case IDC_MAGNIFY_CHECKBOX_STICKY:
        m_bStickyShowing = false;
        break;

    case IDC_MAGNIFY_SLIDER_PIXEL_REGION:
        nTemp = m_pMagnifyUI->GetSlider( IDC_MAGNIFY_SLIDER_PIXEL_REGION )->GetValue();
        swprintf_s( szTemp, L"Magnify Region : %d", nTemp );
        m_pMagnifyUI->GetStatic( IDC_MAGNIFY_STATIC_PIXEL_REGION )->SetText( szTemp );
        m_Magnify.SetPixelRegion( nTemp );
        break;

    case IDC_MAGNIFY_SLIDER_SCALE:
        nTemp = m_pMagnifyUI->GetSlider( IDC_MAGNIFY_SLIDER_SCALE )->GetValue();
        swprintf_s( szTemp, L"Magnify Scale : %d", nTemp );
        m_pMagnifyUI->GetStatic( IDC_MAGNIFY_STATIC_SCALE )->SetText( szTemp );
        m_Magnify.SetScale( nTemp );
        break;
    }
}

bool MagnifyTool::IsEnabled()
{
    return m_pMagnifyUI->GetCheckBox( IDC_MAGNIFY_CHECKBOX_ENABLE )->GetChecked();
}


//--------------------------------------------------------------------------------------
// Private method for UI control
//--------------------------------------------------------------------------------------
void MagnifyTool::EnableTool( bool bEnable )
{
    m_pMagnifyUI->GetCheckBox( IDC_MAGNIFY_CHECKBOX_ENABLE )->SetEnabled( bEnable );
}


void MagnifyTool::EnableUI( bool bEnable )
{
    if (m_pMagnifyUI->GetCheckBox( IDC_MAGNIFY_CHECKBOX_STICKY ))
    {
        m_pMagnifyUI->GetCheckBox( IDC_MAGNIFY_CHECKBOX_STICKY )->SetEnabled( bEnable );
    }
    m_pMagnifyUI->GetStatic( IDC_MAGNIFY_STATIC_PIXEL_REGION )->SetEnabled( bEnable );
    m_pMagnifyUI->GetSlider( IDC_MAGNIFY_SLIDER_PIXEL_REGION )->SetEnabled( bEnable );
    m_pMagnifyUI->GetStatic( IDC_MAGNIFY_STATIC_SCALE )->SetEnabled( bEnable );
    m_pMagnifyUI->GetSlider( IDC_MAGNIFY_SLIDER_SCALE )->SetEnabled( bEnable );
    m_bStickyShowing = false;
}
