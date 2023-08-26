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
// File: ShaderCacheSampleHelper.h
//
// Helpers to implement the DXUT related ShaderCache interface in samples.
//--------------------------------------------------------------------------------------
#ifndef AMD_SDK_SHADER_CACHE_SAMPLE_HELPER_H
#define AMD_SDK_SHADER_CACHE_SAMPLE_HELPER_H


class CDXUTControl;
class CDXUTTextHelper;
typedef long HRESULT;
typedef unsigned int UINT;

namespace AMD
{

    class HUD;
    class ShaderCache;

    //--------------------------------------------------------------------------------------
    // UI control IDs
    //--------------------------------------------------------------------------------------
    extern const int g_MaxApplicationControlID;

    enum SHADER_CACHE_SAMPLE_HELPER_IDC
    {
        AMD_IDC_START = 0,
        AMD_IDC_BUTTON_SHOW_SHADERCACHE_UI       = AMD_IDC_START,
        AMD_IDC_BUTTON_RECOMPILESHADERS_CHANGED,
        AMD_IDC_BUTTON_RECOMPILESHADERS_GLOBAL,
        AMD_IDC_BUTTON_RECREATE_SHADERS,
        AMD_IDC_CHECKBOX_AUTORECOMPILE_SHADERS,
        AMD_IDC_CHECKBOX_SHOW_SHADER_ERRORS,
        AMD_IDC_CHECKBOX_SHOW_ISA_GPR_PRESSURE,
        AMD_IDC_STATIC_TARGET_ISA,
        AMD_IDC_STATIC_TARGET_ISA_INFO,
        AMD_IDC_COMBOBOX_TARGET_ISA,
        AMD_IDC_END
    };

    template< typename T >
    T GetEnum( T i_AMDEnum )
    {
        return static_cast< T > ( g_MaxApplicationControlID + i_AMDEnum );
    }

    void InitApp( ShaderCache& r_ShaderCache, HUD& r_HUD, int& iY, const bool i_bAdvancedShaderCacheGUI_VisibleByDefault = false );
    void ProcessUIChanges();
    void RenderHUDUpdates( CDXUTTextHelper* i_pTxtHelper );
    void __stdcall OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );

} // namespace AMD

#endif
