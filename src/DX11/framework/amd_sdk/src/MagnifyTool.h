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
// File: MagnifyTool.h
//
// MagnifyTool class definition. This class implements a user interface based upon
// the DXUT framework for the Magnify class.
//--------------------------------------------------------------------------------------
#ifndef AMD_SDK_MAGNIFY_TOOL_H
#define AMD_SDK_MAGNIFY_TOOL_H

#include "Magnify.h"

namespace AMD
{

    // GUI defines
    enum MAGNIFY_TOOL_IDC
    {
        IDC_MAGNIFY_STATIC_CAPTION   =  19 + 1024,
        IDC_MAGNIFY_CHECKBOX_ENABLE,
        IDC_MAGNIFY_CHECKBOX_STICKY,
        IDC_MAGNIFY_STATIC_PIXEL_REGION,
        IDC_MAGNIFY_SLIDER_PIXEL_REGION,
        IDC_MAGNIFY_STATIC_SCALE,
        IDC_MAGNIFY_SLIDER_SCALE
    };


    class MagnifyTool
    {
    public:

        // Constructor / destructor
        MagnifyTool();
        ~MagnifyTool();

        // Set methods
        void SetSourceResources( ID3D11Resource* pSourceRTResource, DXGI_FORMAT RTFormat,
            int nWidth, int nHeight, int nSamples );
        void SetPixelRegion( int nPixelRegion ) { m_Magnify.SetPixelRegion( nPixelRegion ); }
        void SetScale( int nScale ) { m_Magnify.SetScale( nScale ); }

        // Hooks for the DX SDK Framework
        void InitApp( CDXUTDialog* pUI, int& iStartHeight, bool bSupportStickyMode = false );
        HRESULT OnCreateDevice( ID3D11Device* pd3dDevice );
        void OnResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain *pSwapChain,
            const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext,
            int nPositionX, int nPositionY );
        void OnDestroyDevice();
        void OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );
        bool IsEnabled();

        // Render
        void Render();

    private:

        // UI helper methods
        void EnableTool( bool bEnable );
        void EnableUI( bool bEnable );

    private:

        // The DXUT dialog
        CDXUTDialog* m_pMagnifyUI;

        // Pointer to the Magnify class
        AMD::Magnify m_Magnify;

        // The source resources
        ID3D11Resource* m_pSourceRTResource;
        DXGI_FORMAT     m_RTFormat;
        int             m_nWidth;
        int             m_nHeight;
        int             m_nSamples;
        bool            m_bReleaseRTOnResize;
        bool            m_bMouseDownLastFrame;
        bool            m_bStickyShowing;
        POINT           m_StickyPoint;
    };

} // namespace AMD

#endif // AMD_SDK_MAGNIFY_TOOL_H
