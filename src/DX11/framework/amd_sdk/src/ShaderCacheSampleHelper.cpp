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
// File: ShaderCacheSampleHelper.cpp
//
// Helpers to implement the DXUT related ShaderCache interface in samples.
//--------------------------------------------------------------------------------------

// DXUT helper code
#include "DXUT.h"
#include "DXUTmisc.h"
#include "DXUTgui.h"
#include "DXUTCamera.h"
#include "DXUTSettingsDlg.h"
#include "SDKmisc.h"
#include "SDKmesh.h"

#include "..\\inc\ShaderCacheSampleHelper.h"

#include "ShaderCache.h"
#include "Sprite.h"
#include "HUD.h"

#pragma warning( disable : 4100 ) // disable unreference formal parameter warnings for /W4 builds

#if !AMD_SDK_PREBUILT_RELEASE_EXE
static void SetHUDVisibility( AMD::HUD& r_HUD, const bool i_bHUDIsVisible )
{
    using namespace AMD;

    assert( AMD_IDC_BUTTON_SHOW_SHADERCACHE_UI == AMD_IDC_START );
    for (int i = AMD_IDC_BUTTON_SHOW_SHADERCACHE_UI + 1; i < AMD_IDC_END; ++i)
    {
        CDXUTControl* pControl = r_HUD.m_GUI.GetControl( GetEnum( i ) );
        if (pControl)
        {
            pControl->SetVisible( i_bHUDIsVisible );
        }
    }
}
#endif

namespace AMD
{
    static bool     g_bAdvancedShaderCacheGUI_IsVisible = false;
    HUD*            g_pHUD = NULL;
    ShaderCache*    g_pShaderCache = NULL;

    void AMD::InitApp( ShaderCache& r_ShaderCache, HUD& r_HUD, int& iY, const bool i_bAdvancedShaderCacheGUI_VisibleByDefault )
    {
#if !AMD_SDK_PREBUILT_RELEASE_EXE
        g_bAdvancedShaderCacheGUI_IsVisible = i_bAdvancedShaderCacheGUI_VisibleByDefault;

        const int i_old_iY = iY;

        g_pHUD = &r_HUD;
        g_pShaderCache = &r_ShaderCache;

        {
            r_HUD.m_GUI.AddButton( GetEnum( AMD_IDC_BUTTON_SHOW_SHADERCACHE_UI ), L"ShaderCache HUD (F9)",
                AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, VK_F9 );

            iY = 0;
            static const int iSCElementOffset = AMD::HUD::iElementOffset - 256;

            r_HUD.m_GUI.AddButton( GetEnum( AMD_IDC_BUTTON_RECOMPILESHADERS_CHANGED ), L"Recompile shaders (F5)",
                iSCElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, VK_F5 );

#if AMD_SDK_INTERNAL_BUILD
            if (r_ShaderCache.GenerateISAGPRPressure())
            {
                r_HUD.m_GUI.AddButton( GetEnum( AMD_IDC_BUTTON_RECREATE_SHADERS ), L"Gen |ISA| shaders (F6)",
                    iSCElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, VK_F6 );
            }
#endif

            r_HUD.m_GUI.AddButton( GetEnum( AMD_IDC_BUTTON_RECOMPILESHADERS_GLOBAL ), L"Build ALL shaders (F7)",
                iSCElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, VK_F7 );

            r_HUD.m_GUI.AddCheckBox( GetEnum( AMD_IDC_CHECKBOX_AUTORECOMPILE_SHADERS ), L"Auto Recompile Shaders",
                iSCElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, r_ShaderCache.RecompileTouchedShaders() );

            if (r_ShaderCache.ShaderErrorDisplayType() == ShaderCache::ERROR_DISPLAY_ON_SCREEN)
            {
                r_HUD.m_GUI.AddCheckBox( GetEnum( AMD_IDC_CHECKBOX_SHOW_SHADER_ERRORS ), L"Show Compiler Errors",
                    iSCElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, r_ShaderCache.ShowShaderErrors() );
            }

#if AMD_SDK_INTERNAL_BUILD
            if (r_ShaderCache.GenerateISAGPRPressure())
            {
                r_HUD.m_GUI.AddCheckBox( GetEnum( AMD_IDC_CHECKBOX_SHOW_ISA_GPR_PRESSURE ), L"Show ISA GPR Pressure",
                    iSCElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, r_ShaderCache.ShowISAGPRPressure() );
                r_HUD.m_GUI.AddStatic( GetEnum( AMD_IDC_STATIC_TARGET_ISA ), L"Target ISA:",
                    iSCElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight );

                CDXUTComboBox *pCombo;
                r_HUD.m_GUI.AddComboBox( GetEnum( AMD_IDC_COMBOBOX_TARGET_ISA ),
                    iSCElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, 0, true, &pCombo );

                if (pCombo)
                {
                    pCombo->SetDropHeight( 300 );
                    for (int i = AMD::FIRST_ISA_TARGET; i < AMD::NUM_ISA_TARGETS; ++i)
                    {
                        pCombo->AddItem( AMD::AmdTargetInfo[i].m_Name, NULL );
                    }
                    pCombo->SetSelectedByIndex( AMD::DEFAULT_ISA_TARGET );
                }
                r_HUD.m_GUI.AddStatic( GetEnum( AMD_IDC_STATIC_TARGET_ISA_INFO ), L"Press (F6) to add New ISA",
                    iSCElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight );
            }
#endif
        }

        SetHUDVisibility( r_HUD, i_bAdvancedShaderCacheGUI_VisibleByDefault );

        iY = i_old_iY + AMD::HUD::iElementDelta;
#endif
    }

    void AMD::ProcessUIChanges()
    {
#if !AMD_SDK_PREBUILT_RELEASE_EXE
        if (g_pHUD == NULL || g_pShaderCache == NULL)
        {
            return;
        }

        ShaderCache& r_ShaderCache = *g_pShaderCache;
        HUD& r_HUD = *g_pHUD;

        r_ShaderCache.SetRecompileTouchedShadersFlag( r_HUD.m_GUI.GetCheckBox( GetEnum( AMD_IDC_CHECKBOX_AUTORECOMPILE_SHADERS ) )->GetChecked() );
        if (r_ShaderCache.ShaderErrorDisplayType() == ShaderCache::ERROR_DISPLAY_ON_SCREEN)
        {
            r_ShaderCache.SetShowShaderErrorsFlag( r_HUD.m_GUI.GetCheckBox( GetEnum( AMD_IDC_CHECKBOX_SHOW_SHADER_ERRORS ) )->GetChecked() );
        }
#if AMD_SDK_INTERNAL_BUILD
        if (r_ShaderCache.GenerateISAGPRPressure())
        {
            r_ShaderCache.SetShowShaderISAFlag( r_HUD.m_GUI.GetCheckBox( GetEnum( AMD_IDC_CHECKBOX_SHOW_ISA_GPR_PRESSURE ) )->GetChecked() );
        }
#endif
#endif
    }

    void AMD::RenderHUDUpdates( CDXUTTextHelper* i_pTxtHelper )
    {
#if !AMD_SDK_PREBUILT_RELEASE_EXE
        if (g_pHUD == NULL || g_pShaderCache == NULL)
        {
            return;
        }

        ShaderCache& r_ShaderCache = *g_pShaderCache;

        if (r_ShaderCache.ShadersReady() || (r_ShaderCache.ShowShaderErrors() && r_ShaderCache.HasErrorsToDisplay()))
        {
            assert( i_pTxtHelper );
            r_ShaderCache.RenderShaderErrors( i_pTxtHelper, 15, DirectX::XMVectorSet( 1.0f, 1.0f, 0.0f, 1.0f ) );
#if AMD_SDK_INTERNAL_BUILD
            if (r_ShaderCache.GenerateISAGPRPressure())
            {
                r_ShaderCache.RenderISAInfo( i_pTxtHelper, 15, DirectX::XMVectorSet( 1.0f, 1.0f, 0.0f, 1.0f ) );
            }
#endif
        }
#endif
    }

    //--------------------------------------------------------------------------------------
    // Handles the GUI events
    //--------------------------------------------------------------------------------------
    void __stdcall AMD::OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
    {
#if !AMD_SDK_PREBUILT_RELEASE_EXE
        if (g_pHUD == NULL || g_pShaderCache == NULL)
        {
            return;
        }

        ShaderCache& r_ShaderCache = *g_pShaderCache;
        HUD& r_HUD = *g_pHUD;

        if (nControlID == GetEnum( AMD_IDC_BUTTON_RECOMPILESHADERS_CHANGED ))
        {
            // Errors will post during compile, don't need them right now
            const bool bOldShaderErrorsFlag = r_ShaderCache.ShowShaderErrors();
            r_ShaderCache.SetShowShaderErrorsFlag( false );
            r_ShaderCache.GenerateShaders( AMD::ShaderCache::CREATE_TYPE_COMPILE_CHANGES, true );
            r_ShaderCache.SetShowShaderErrorsFlag( bOldShaderErrorsFlag );
        }
        else if (nControlID == GetEnum( AMD_IDC_BUTTON_RECOMPILESHADERS_GLOBAL ))
        {
            // Errors will post during compile, don't need them right now
            const bool bOldShaderErrorsFlag = r_ShaderCache.ShowShaderErrors();
            r_ShaderCache.SetShowShaderErrorsFlag( false );
            r_ShaderCache.GenerateShaders( AMD::ShaderCache::CREATE_TYPE_FORCE_COMPILE, true );
            r_ShaderCache.SetShowShaderErrorsFlag( bOldShaderErrorsFlag );
        }
#if AMD_SDK_INTERNAL_BUILD
        else if (nControlID == GetEnum( AMD_IDC_BUTTON_RECREATE_SHADERS ))
        {
            assert( r_ShaderCache.GenerateISAGPRPressure() ); // Shouldn't call this if we aren't using ISA
            const bool k_bOK = r_ShaderCache.CloneShaders();
            assert( k_bOK );
            if (k_bOK)
            {
                // Errors won't post using this method... Should find a better one!
                // Need to somehow toggle error rendering off and then on again next frame...
                const bool bOldShaderErrorsFlag = r_ShaderCache.ShowShaderErrors();
                r_ShaderCache.SetShowShaderErrorsFlag( false );
                r_ShaderCache.GenerateShaders( AMD::ShaderCache::CREATE_TYPE_USE_CACHED, true );
                r_ShaderCache.SetShowShaderErrorsFlag( bOldShaderErrorsFlag );
            }
        }
        else if (nControlID == GetEnum( AMD_IDC_COMBOBOX_TARGET_ISA ))
        {
            assert( r_ShaderCache.GenerateISAGPRPressure() ); // Shouldn't call this if we aren't using ISA
            r_ShaderCache.SetTargetISA( (AMD::ISA_TARGET)((CDXUTComboBox*)pControl)->GetSelectedIndex() );
        }
#endif
        else if (nControlID == GetEnum( AMD_IDC_BUTTON_SHOW_SHADERCACHE_UI ))
        {
            // Toggle Render of ShaderCache GUI
            g_bAdvancedShaderCacheGUI_IsVisible = !g_bAdvancedShaderCacheGUI_IsVisible;
            SetHUDVisibility( r_HUD, g_bAdvancedShaderCacheGUI_IsVisible );
        }
#endif
    }

} // namespace AMD
