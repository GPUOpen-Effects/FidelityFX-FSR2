// This file is part of the FidelityFX SDK.
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

#include "stdafx.h"

#include "UI.h"
#include "FSR2Sample.h"
#include "imgui.h"
#include "base/FrameworkWindows.h"

// To use the 'disabled UI state' functionality (ImGuiItemFlags_Disabled), include internal header
// https://github.com/ocornut/imgui/issues/211#issuecomment-339241929
#include "imgui_internal.h"
static void DisableUIStateBegin(const bool& bEnable)
{
    if (!bEnable)
    {
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
    }
};
static void DisableUIStateEnd(const bool& bEnable)
{
    if (!bEnable)
    {
        ImGui::PopItemFlag();
        ImGui::PopStyleVar();
    }
};

// Some constants and utility functions
static constexpr float MAGNIFICATION_AMOUNT_MIN = 1.0f;
static constexpr float MAGNIFICATION_AMOUNT_MAX = 32.0f;
static constexpr float MAGNIFIER_RADIUS_MIN = 0.01f;
static constexpr float MAGNIFIER_RADIUS_MAX = 0.85f;
static constexpr float MAGNIFIER_BORDER_COLOR__LOCKED[3] = { 0.002f, 0.72f, 0.0f }; // G
static constexpr float MAGNIFIER_BORDER_COLOR__FREE  [3] = { 0.72f, 0.002f, 0.0f }; // R
template<class T> static T clamped(const T& v, const T& min, const T& max)
{
    if (v < min)      return min;
    else if (v > max) return max;
    else              return v;
}

void FSR2Sample::BuildUI()
{
    // save current values in case anything changes in the UI
    m_SavedUiValues[m_UIState.m_nUpscaleType].scaleMode = m_nUpscaleMode;
    m_SavedUiValues[m_UIState.m_nUpscaleType].upscaleRatio = m_fUpscaleRatio;
    m_SavedUiValues[m_UIState.m_nUpscaleType].mipBias = m_UIState.mipBias;
    m_SavedUiValues[m_UIState.m_nUpscaleType].useSharpen = m_UIState.bUseRcas;
    m_SavedUiValues[m_UIState.m_nUpscaleType].sharpness = m_UIState.sharpening;
    m_SavedUiValues[m_UIState.m_nUpscaleType].useTAA = m_UIState.bUseTAA;

    // if we haven't initialized GLTFLoader yet, don't draw UI.
    if (m_pGltfLoader == nullptr)
    {
        LoadScene(m_UIState.m_activeScene);
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle& style = ImGui::GetStyle();
    style.FrameBorderSize = 1.0f;

    const uint32_t W = this->GetWidth();
    const uint32_t H = this->GetHeight();

    const uint32_t PROFILER_WINDOW_PADDIG_X = 10;
    const uint32_t PROFILER_WINDOW_PADDIG_Y = 10;
    const uint32_t PROFILER_WINDOW_SIZE_X = 330;
    const uint32_t PROFILER_WINDOW_SIZE_Y = 450;
    const uint32_t PROFILER_WINDOW_POS_X = W - PROFILER_WINDOW_PADDIG_X - PROFILER_WINDOW_SIZE_X;
    const uint32_t PROFILER_WINDOW_POS_Y = PROFILER_WINDOW_PADDIG_Y;

    const uint32_t CONTROLS_WINDOW_POS_X = 10;
    const uint32_t CONTROLS_WINDOW_POS_Y = 10;
    const uint32_t CONTROLW_WINDOW_SIZE_X = 350;
    const uint32_t CONTROLW_WINDOW_SIZE_Y = 780; // assuming > 720p

    // Render CONTROLS window
    //
    ImGui::SetNextWindowPos(ImVec2(CONTROLS_WINDOW_POS_X, CONTROLS_WINDOW_POS_Y), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(CONTROLW_WINDOW_SIZE_X, CONTROLW_WINDOW_SIZE_Y), ImGuiCond_FirstUseEver);

    if (m_UIState.bShowControlsWindow)
    {
        ImGui::Begin("CONTROLS (F11)", &m_UIState.bShowControlsWindow);
        if (ImGui::CollapsingHeader("Animation", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Checkbox("Play", &m_bPlay);
            ImGui::SliderFloat("Time", &m_time, 0, 30);
        }

        ImGui::Spacing();
        ImGui::Spacing();

        if (ImGui::CollapsingHeader("Scene", ImGuiTreeNodeFlags_DefaultOpen))
        {
            char* cameraControl[] = { "Orbit", "WASD", "cam #0", "cam #1", "cam #2", "cam #3" , "cam #4", "cam #5" };

            if (m_activeCamera >= m_pGltfLoader->m_cameras.size() + 2)
                m_activeCamera = 0;
            ImGui::Combo("Camera", &m_activeCamera, cameraControl, min((int)(m_pGltfLoader->m_cameras.size() + 2), _countof(cameraControl)));
            ImGui::Checkbox("Camera Headbobbing", &m_UIState.m_bHeadBobbing);

            auto getterLambda = [](void* data, int idx, const char** out_str)->bool { *out_str = ((std::vector<std::string> *)data)->at(idx).c_str(); return true; };
            if (ImGui::Combo("Model", &m_UIState.m_activeScene, getterLambda, &m_sceneNames, (int)m_sceneNames.size()))
            {
                m_UIState.bRenderAnimatedTextures = (m_UIState.m_activeScene == 1);
                // Note:
                // probably queueing this as an event and handling it at the end/beginning 
                // of frame is a better idea rather than in the middle of drawing UI.
                LoadScene(m_UIState.m_activeScene);

                //bail out as we need to reload everything
                ImGui::End();
                ImGui::EndFrame();
                ImGui::NewFrame();
                return;
            }

            ImGui::SliderFloat("IBL Factor", &m_UIState.IBLFactor, 0.0f, 3.0f);
            for (int i = 0; i < m_pGltfLoader->m_lights.size(); i++)
            {
                ImGui::SliderFloat(format("Light %i Intensity", i).c_str(), &m_pGltfLoader->m_lights[i].m_intensity, 0.0f, 50.0f);
            }

            if (ImGui::Button("Set Spot Light 0 to Camera's View"))
            {
                int idx = m_pGltfLoader->m_lightInstances[0].m_nodeIndex;
                m_pGltfLoader->m_nodes[idx].m_transform.LookAt(m_UIState.camera.GetPosition(), m_UIState.camera.GetPosition() - m_UIState.camera.GetDirection());
                m_pGltfLoader->m_animatedMats[idx] = m_pGltfLoader->m_nodes[idx].m_transform.GetWorldMat();
            }

        }

        if (m_UIState.m_balloonID >= 0) {

            if (ImGui::CollapsingHeader("Balloon Control", ImGuiTreeNodeFlags_DefaultOpen)) {

                ImGui::Checkbox("Attach to camera", &m_UIState.m_bBalloonAttachToCamera);
                ImGui::SliderFloat("Amplitude", &m_UIState.m_BalloonAmplitude, 0.0f, 1.0f);
                ImGui::SliderFloat("Offset X", &m_UIState.balloon_offset_x, -1.0f, 1.0f);
                ImGui::SliderFloat("Offset Y", &m_UIState.balloon_offset_y, -1.0f, 1.0f);
                ImGui::SliderFloat("Offset Z", &m_UIState.balloon_offset_z, -1.0f, 1.0f);
                ImGui::SliderFloat("Scale", &m_UIState.balloon_offset_scale, 0.0f, 0.1f);
            }
        }

        ImGui::Spacing();
        ImGui::Spacing();
        if (ImGui::CollapsingHeader("Upscaling ", ImGuiTreeNodeFlags_DefaultOpen)) //blank space needed, otherwise the identically named 'combo' drop down below won't work
        {
            const char* modes[] = { "Point [Ctrl-1]", "Bilinear [Ctrl-2]", "Bicubic [Ctrl-3]", "FSR 1.0 [Ctrl-4]", "FSR 2.0 [Ctrl-5]", "Native [Ctrl-0]" };
            if (ImGui::Combo("Upscaling", (int32_t*)&m_UIState.m_nUpscaleType, modes, _countof(modes)))
            {
                if (m_UIState.m_nUpscaleType == UPSCALE_TYPE_NATIVE)
                {
                    // force reset values for native
                    m_SavedUiValues[m_UIState.m_nUpscaleType].scaleMode = UPSCALE_QUALITY_MODE_NONE;
                    m_SavedUiValues[m_UIState.m_nUpscaleType].upscaleRatio = 1.f;
                    m_SavedUiValues[m_UIState.m_nUpscaleType].mipBias = 0.f;
                }
                
                // update current values based on the last stored defaults
                m_nUpscaleMode = m_SavedUiValues[m_UIState.m_nUpscaleType].scaleMode;
                m_fUpscaleRatio = m_SavedUiValues[m_UIState.m_nUpscaleType].upscaleRatio;
                m_UIState.mipBias = m_SavedUiValues[m_UIState.m_nUpscaleType].mipBias;
                m_UIState.bUseRcas = m_SavedUiValues[m_UIState.m_nUpscaleType].useSharpen;
                m_UIState.sharpening = m_SavedUiValues[m_UIState.m_nUpscaleType].sharpness;
                m_UIState.bUseTAA = m_SavedUiValues[m_UIState.m_nUpscaleType].useTAA;

                RefreshRenderResolution();
                OnResize(true);
            }


            if (m_UIState.m_nUpscaleType == UPSCALE_TYPE_FSR_2_0)
            {
                // adjust to match the combo box options
                int32_t upscaleQualityMode = m_nUpscaleMode - UPSCALE_QUALITY_MODE_QUALITY;

                const char* ratios[] = { "Quality (1.5x)", "Balanced (1.7x)", "Performance (2x)", "Ultra Performance (3x)", "Custom" };
                if (ImGui::Combo("Scale mode", &upscaleQualityMode, ratios, _countof(ratios)))
                {
                    m_nUpscaleMode = (UpscaleQualityMode)(upscaleQualityMode + UPSCALE_QUALITY_MODE_QUALITY); // above combo box starts from quality mode.
                    m_UIState.mipBias = mipBias[m_nUpscaleMode];

                    RefreshRenderResolution();
                    OnResize();
                }
                else
                {
                    m_nUpscaleMode = (UpscaleQualityMode)(upscaleQualityMode + UPSCALE_QUALITY_MODE_QUALITY); // above combo box starts from quality mode.
                    ImGui::SliderFloat("Mip LOD bias", &m_UIState.mipBias, -5.0f, 0.0f);
                }

                if ((m_nUpscaleMode == UPSCALE_QUALITY_MODE_CUSTOM) && ImGui::SliderFloat("Custom factor", &m_fUpscaleRatio, 1.0f, 3.0f))
                {
                    RefreshRenderResolution();
                    OnResize();
                }


                if (ImGui::Checkbox("Dynamic resolution", &m_UIState.bDynamicRes)) 
                {
                    OnResize();
                }

                const char* reactiveOptions[] = { "Disabled", "Manual Reactive Mask Generation", "Autogen FSR2 Helper Function" };
                ImGui::Combo("Reactive Mask mode", (int*)(&m_UIState.nReactiveMaskMode), reactiveOptions, _countof(reactiveOptions));

                ImGui::Checkbox("Use Transparency and Composition Mask", &m_UIState.bCompositionMask);

                if (m_pRenderer &&
                    ImGui::CollapsingHeader("Dev Options", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    m_pRenderer->BuildDevUI(&m_UIState);
                }
            }
            else if (m_UIState.m_nUpscaleType <= UPSCALE_TYPE_FSR_1_0)
            {
                const char* ratios[] = { "Native", "Ultra Quality (1.3x)", "Quality (1.5x)", "Balanced (1.7x)", "Performance (2x)", "Ultra Performance (3x)", "Custom" };
                if (ImGui::Combo("Scale mode", (int32_t*)&m_nUpscaleMode, ratios, _countof(ratios)))
                {
                    m_UIState.mipBias = mipBias[m_nUpscaleMode];

                    RefreshRenderResolution();
                    OnResize();
                }
                else
                {
                    ImGui::SliderFloat("Mip LOD bias", &m_UIState.mipBias, -5.0f, 0.0f);
                }


                if ((m_nUpscaleMode == UPSCALE_QUALITY_MODE_CUSTOM) && ImGui::SliderFloat("Custom factor", &m_fUpscaleRatio, 1.0f, 3.0f))
                {
                    RefreshRenderResolution();
                    OnResize();
                }

                m_UIState.bDynamicRes = false;
            }
            else
            {
                m_UIState.mipBias = mipBias[UPSCALE_QUALITY_MODE_NONE];
            }

            if (m_UIState.m_nUpscaleType != UPSCALE_TYPE_FSR_2_0)
            {
                m_UIState.bDynamicRes = false;
            }

            ImGui::Checkbox("RCAS Sharpening", &m_UIState.bUseRcas);

            if (m_UIState.m_nUpscaleType == UPSCALE_TYPE_FSR_2_0) {
                if (m_UIState.bUseRcas) {
                    ImGui::SliderFloat("Sharpening", &m_UIState.sharpening, 0.0f, 1.0f);
                }
            } else {
                if (m_UIState.bUseRcas) {
                    ImGui::SliderFloat("Sharpening attentuation", &m_UIState.sharpening, 0.0f, 10.0f);
                }
            }

            if (m_UIState.m_nUpscaleType != UPSCALE_TYPE_FSR_2_0)
                ImGui::Checkbox("TAA", &m_UIState.bUseTAA);

            ImGui::Text("Render resolution: %dx%d", m_UIState.renderWidth, m_UIState.renderHeight);
            ImGui::Text("Display resolution: %dx%d", m_UIState.displayWidth, m_UIState.displayHeight);
            if (m_bDisplayHalf)
            {
                ImGui::Spacing();
                ImGui::Spacing();
                ImGui::Text(UseSlowFallback(m_device.IsFp16Supported()) ? "Slow fallback" : "Fast path");
            }
        }

        ImGui::Spacing();
        ImGui::Spacing();
        if (ImGui::CollapsingHeader("PostProcessing", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const char* tonemappers[] = { "AMD Tonemapper", "DX11DSK", "Reinhard", "Uncharted2Tonemap", "ACES", "No tonemapper" };
            ImGui::Combo("Tonemapper", &m_UIState.SelectedTonemapperIndex, tonemappers, _countof(tonemappers));

            bool isHdr = m_displayModesAvailable[m_currentDisplayModeNamesIndex] != DISPLAYMODE_SDR;
            ImGui::SliderFloat("Exposure", isHdr ? &m_UIState.ExposureHdr : &m_UIState.Exposure, 0.0f, 4.0f);
        }

        ImGui::Spacing();
        ImGui::Spacing();

        if (ImGui::CollapsingHeader("Magnifier", ImGuiTreeNodeFlags_DefaultOpen))
        {
            // read in Magnifier pass parameters from the UI & app state
            MagnifierPS::PassParameters& params = m_UIState.MagnifierParams;
            params.uImageHeight = m_Height;
            params.uImageWidth = m_Width;
            params.iMousePos[0] = m_UIState.bLockMagnifierPosition ? m_UIState.LockedMagnifiedScreenPositionX : static_cast<int>(io.MousePos.x);
            params.iMousePos[1] = m_UIState.bLockMagnifierPosition ? m_UIState.LockedMagnifiedScreenPositionY : static_cast<int>(io.MousePos.y);

            ImGui::Checkbox("Show Magnifier (M)", &m_UIState.bUseMagnifier);

            DisableUIStateBegin(m_UIState.bUseMagnifier);
            {
                // Use a local bool state here to track locked state through the UI widget,
                // and then call ToggleMagnifierLockedState() to update the persistent state (m_UIstate).
                // The keyboard input for toggling lock directly operates on the persistent state.
                const bool bIsMagnifierCurrentlyLocked = m_UIState.bLockMagnifierPosition;
                bool bMagnifierToggle = bIsMagnifierCurrentlyLocked;
                ImGui::Checkbox("Lock Position (L)", &bMagnifierToggle);
                
                if (bMagnifierToggle != bIsMagnifierCurrentlyLocked)
                    m_UIState.ToggleMagnifierLock();

                ImGui::SliderFloat("Screen Size", &params.fMagnifierScreenRadius, MAGNIFIER_RADIUS_MIN, MAGNIFIER_RADIUS_MAX);
                ImGui::SliderFloat("Magnification", &params.fMagnificationAmount, MAGNIFICATION_AMOUNT_MIN, MAGNIFICATION_AMOUNT_MAX);
                ImGui::SliderInt("OffsetX", &params.iMagnifierOffset[0], -m_Width, m_Width);
                ImGui::SliderInt("OffsetY", &params.iMagnifierOffset[1], -m_Height, m_Height);
            }
            DisableUIStateEnd(m_UIState.bUseMagnifier);
        }

        ImGui::Spacing();
        ImGui::Spacing();

        if (ImGui::CollapsingHeader("Debug", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Checkbox("Show Bounding Boxes", &m_UIState.bDrawBoundingBoxes);
            ImGui::Checkbox("Show Light Frustum", &m_UIState.bDrawLightFrustum);
            
            ImGui::Text("Wireframe");
            ImGui::SameLine(); ImGui::RadioButton("Off", (int*)&m_UIState.WireframeMode, (int)UIState::WireframeMode::WIREFRAME_MODE_OFF);
            ImGui::SameLine(); ImGui::RadioButton("Shaded", (int*)&m_UIState.WireframeMode, (int)UIState::WireframeMode::WIREFRAME_MODE_SHADED);
            ImGui::SameLine(); ImGui::RadioButton("Solid color", (int*)&m_UIState.WireframeMode, (int)UIState::WireframeMode::WIREFRAME_MODE_SOLID_COLOR);
            if (m_UIState.WireframeMode == UIState::WireframeMode::WIREFRAME_MODE_SOLID_COLOR)
                ImGui::ColorEdit3("Wire solid color", m_UIState.WireframeColor, ImGuiColorEditFlags_NoAlpha);
        }

        ImGui::Spacing();
        ImGui::Spacing();

        if (ImGui::CollapsingHeader("Presentation Mode", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const char* fullscreenModes[] = { "Windowed", "BorderlessFullscreen", "ExclusiveFullscreen" };
            if (ImGui::Combo("Fullscreen Mode", (int*)&m_fullscreenMode, fullscreenModes, _countof(fullscreenModes)))
            {
                if (m_previousFullscreenMode != m_fullscreenMode)
                {
                    HandleFullScreen();
                    m_previousFullscreenMode = m_fullscreenMode;
                }
            }

            ImGui::Checkbox("Framerate Limiter", &m_UIState.bEnableFrameRateLimiter);
            if (m_UIState.bEnableFrameRateLimiter)
            {
                ImGui::Checkbox("Use GPU Framerate Limiter", &m_UIState.bUseGPUFrameRateLimiter);
                DWORD maxFramerate = 240;
                DEVMODE lpDevMode = {};
                lpDevMode.dmSize = sizeof(DEVMODE);

                static int sTargetFPS = -1;

                if (sTargetFPS < 0)
                {
                    if (EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &lpDevMode))
                    {
                        sTargetFPS = lpDevMode.dmDisplayFrequency;
                    }
                    else
                    {
                        sTargetFPS = maxFramerate;
                    }
                }

                ImGui::SliderInt("Target FPS", &sTargetFPS, 5, maxFramerate);
                m_UIState.targetFrameTime = 1000.f * (1.f / sTargetFPS);
            }
        }

        ImGui::Spacing();
        ImGui::Spacing();

        if (m_FreesyncHDROptionEnabled && ImGui::CollapsingHeader("FreeSync HDR", ImGuiTreeNodeFlags_DefaultOpen))
        {
            static bool openWarning = false;
            const char** displayModeNames = &m_displayModesNamesAvailable[0];
            if (ImGui::Combo("Display Mode", (int*)&m_currentDisplayModeNamesIndex, displayModeNames, (int)m_displayModesNamesAvailable.size()))
            {
                if (m_fullscreenMode != PRESENTATIONMODE_WINDOWED)
                {
                    UpdateDisplay(m_disableLocalDimming);
                    m_previousDisplayModeNamesIndex = m_currentDisplayModeNamesIndex;
                }
                else if (CheckIfWindowModeHdrOn() &&
                    (m_displayModesAvailable[m_currentDisplayModeNamesIndex] == DISPLAYMODE_SDR ||
                        m_displayModesAvailable[m_currentDisplayModeNamesIndex] == DISPLAYMODE_HDR10_2084 ||
                        m_displayModesAvailable[m_currentDisplayModeNamesIndex] == DISPLAYMODE_HDR10_SCRGB))
                {
                    UpdateDisplay(m_disableLocalDimming);
                    m_previousDisplayModeNamesIndex = m_currentDisplayModeNamesIndex;
                }
                else
                {
                    openWarning = true;
                    m_currentDisplayModeNamesIndex = m_previousDisplayModeNamesIndex;
                }
                OnResize(true);
            }

            if (openWarning)
            {
                ImGui::OpenPopup("Display Modes Warning");
                ImGui::BeginPopupModal("Display Modes Warning", NULL, ImGuiWindowFlags_AlwaysAutoResize);
                ImGui::Text("\nChanging display modes is only available either using HDR toggle in windows display setting for HDR10 modes or in fullscreen for FS HDR modes\n\n");
                if (ImGui::Button("Cancel", ImVec2(120, 0))) { openWarning = false; ImGui::CloseCurrentPopup(); }
                ImGui::EndPopup();
            }

            if (m_displayModesAvailable[m_currentDisplayModeNamesIndex] == DisplayMode::DISPLAYMODE_FSHDR_Gamma22 ||
                m_displayModesAvailable[m_currentDisplayModeNamesIndex] == DisplayMode::DISPLAYMODE_FSHDR_SCRGB)
            {
                static bool selectedDisableLocaldimmingSetting = false;
                if (ImGui::Checkbox("Disable Local Dimming", &selectedDisableLocaldimmingSetting))
                    UpdateDisplay(selectedDisableLocaldimmingSetting);
            }
        }

        ImGui::End(); // CONTROLS
    }


    // Render PROFILER window
    //
    if (m_UIState.bShowProfilerWindow)
    {
        constexpr size_t NUM_FRAMES = 128;
        static float FRAME_TIME_ARRAY[NUM_FRAMES] = { 0 };

        // track highest frame rate and determine the max value of the graph based on the measured highest value
        static float RECENT_HIGHEST_FRAME_TIME = 0.0f;
        constexpr int FRAME_TIME_GRAPH_MAX_FPS[] = { 800, 240, 120, 90, 65, 45, 30, 15, 10, 5, 4, 3, 2, 1 };
        static float  FRAME_TIME_GRAPH_MAX_VALUES[_countof(FRAME_TIME_GRAPH_MAX_FPS)] = { 0 }; // us
        for (int i = 0; i < _countof(FRAME_TIME_GRAPH_MAX_FPS); ++i) { FRAME_TIME_GRAPH_MAX_VALUES[i] = 1000000.f / FRAME_TIME_GRAPH_MAX_FPS[i]; }

        //scrolling data and average FPS computing
        const std::vector<TimeStamp>& timeStamps = m_pRenderer->GetTimingValues();
        const bool bTimeStampsAvailable = timeStamps.size() > 0;
        if (bTimeStampsAvailable)
        {
            RECENT_HIGHEST_FRAME_TIME = 0;
            FRAME_TIME_ARRAY[NUM_FRAMES - 1] = timeStamps.back().m_microseconds;
            for (uint32_t i = 0; i < NUM_FRAMES - 1; i++)
            {
                FRAME_TIME_ARRAY[i] = FRAME_TIME_ARRAY[i + 1];
            }
            RECENT_HIGHEST_FRAME_TIME = max(RECENT_HIGHEST_FRAME_TIME, FRAME_TIME_ARRAY[NUM_FRAMES - 1]);
        }
        const float& frameTime_us = FRAME_TIME_ARRAY[NUM_FRAMES - 1];
        const float  frameTime_ms = frameTime_us * 0.001f;
        const int fps = bTimeStampsAvailable ? static_cast<int>(1000000.0f / frameTime_us) : 0;

        // UI
        ImGui::SetNextWindowPos(ImVec2((float)PROFILER_WINDOW_POS_X, (float)PROFILER_WINDOW_POS_Y), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(PROFILER_WINDOW_SIZE_X, PROFILER_WINDOW_SIZE_Y), ImGuiCond_FirstUseEver);
        ImGui::Begin("PROFILER (F12))", &m_UIState.bShowProfilerWindow);

        ImGui::Text("Resolution : %ix%i", m_Width, m_Height);
        ImGui::Text("API        : %s", m_systemInfo.mGfxAPI.c_str());
        ImGui::Text("GPU        : %s", m_systemInfo.mGPUName.c_str());
        ImGui::Text("CPU        : %s", m_systemInfo.mCPUName.c_str());
        ImGui::Text("FPS        : %d (%.2f ms)", fps, frameTime_ms);

        if (ImGui::CollapsingHeader("GPU Timings", ImGuiTreeNodeFlags_DefaultOpen))
        {
            std::string msOrUsButtonText = m_UIState.bShowMilliseconds ? "Switch to microseconds" : "Switch to milliseconds";
            if (ImGui::Button(msOrUsButtonText.c_str())) {
                m_UIState.bShowMilliseconds = !m_UIState.bShowMilliseconds;
            }
            ImGui::Checkbox("Show Avg/Min/Max", &m_UIState.bShowAverage);
            ImGui::Spacing();

            if (m_isCpuValidationLayerEnabled || m_isGpuValidationLayerEnabled)
            {
                ImGui::TextColored(ImVec4(1,1,0,1), "WARNING: Validation layer is switched on"); 
                ImGui::Text("Performance numbers may be inaccurate!");
            }

            // find the index of the FrameTimeGraphMaxValue as the next higher-than-recent-highest-frame-time in the pre-determined value list
            size_t iFrameTimeGraphMaxValue = 0;
            for (int i = 0; i < _countof(FRAME_TIME_GRAPH_MAX_VALUES); ++i)
            {
                if (RECENT_HIGHEST_FRAME_TIME < FRAME_TIME_GRAPH_MAX_VALUES[i]) // FRAME_TIME_GRAPH_MAX_VALUES are in increasing order
                {
                    iFrameTimeGraphMaxValue = min(_countof(FRAME_TIME_GRAPH_MAX_VALUES) - 1, i + 1);
                    break;
                }
            }
            ImGui::PlotLines("", FRAME_TIME_ARRAY, NUM_FRAMES, 0, "GPU frame time (us)", 0.0f, FRAME_TIME_GRAPH_MAX_VALUES[iFrameTimeGraphMaxValue], ImVec2(0, 80));

            constexpr uint32_t AvgTimeStampUpdateIntervalMs = 1000;
            const double currTime = MillisecondsNow();

            bool bResetAccumulatingState = false;

            if ((m_UIState.accumulatingFrameCount > 1) &&
                ((currTime - m_UIState.lastTimeStampsResetTime) > AvgTimeStampUpdateIntervalMs))
            {
                std::swap(m_UIState.displayedTimeStamps, m_UIState.accumulatingTimeStamps);

                for (uint32_t i = 0; i < m_UIState.displayedTimeStamps.size(); i++)
                {
                    m_UIState.displayedTimeStamps[i].sum /= m_UIState.accumulatingFrameCount;
                }

                bResetAccumulatingState = true;
            }

            bResetAccumulatingState |= (m_UIState.accumulatingTimeStamps.size() != timeStamps.size());

            if (bResetAccumulatingState)
            {
                m_UIState.accumulatingTimeStamps.resize(0);
                m_UIState.accumulatingTimeStamps.resize(timeStamps.size());
                m_UIState.lastTimeStampsResetTime = currTime;
                m_UIState.accumulatingFrameCount = 0;
            }

            const float unitScaling = m_UIState.bShowMilliseconds ? (1.0f / 1000.0f) : 1.0f;

            for (uint32_t i = 0; i < timeStamps.size(); i++)
            {
                float value = timeStamps[i].m_microseconds * unitScaling;
                const char* pStrUnit = m_UIState.bShowMilliseconds ? "ms" : "us";
                ImGui::Text("%-18s: %7.2f %s", timeStamps[i].m_label.c_str(), value, pStrUnit);

                if (m_UIState.bShowAverage)
                {
                    if (m_UIState.displayedTimeStamps.size() == timeStamps.size())
                    {
                        ImGui::SameLine();
                        ImGui::Text("  avg: %7.2f %s", m_UIState.displayedTimeStamps[i].sum * unitScaling, pStrUnit);
                        ImGui::SameLine();
                        ImGui::Text("  min: %7.2f %s", m_UIState.displayedTimeStamps[i].minimum * unitScaling, pStrUnit);
                        ImGui::SameLine();
                        ImGui::Text("  max: %7.2f %s", m_UIState.displayedTimeStamps[i].maximum * unitScaling, pStrUnit);
                    }

                    UIState::AccumulatedTimeStamp* pAccumulatingTimeStamp = &m_UIState.accumulatingTimeStamps[i];
                    pAccumulatingTimeStamp->sum += timeStamps[i].m_microseconds;
                    pAccumulatingTimeStamp->minimum = min(pAccumulatingTimeStamp->minimum, timeStamps[i].m_microseconds);
                    pAccumulatingTimeStamp->maximum = max(pAccumulatingTimeStamp->maximum, timeStamps[i].m_microseconds);
                }
            }

            m_UIState.accumulatingFrameCount++;
        }
        ImGui::End(); // PROFILER
    }
}

void UIState::Initialize()
{
    // init magnifier params
    for (int ch = 0; ch < 3; ++ch) this->MagnifierParams.fBorderColorRGB[ch] = MAGNIFIER_BORDER_COLOR__FREE[ch]; // start at 'free' state

    // init GUI state
    this->SelectedTonemapperIndex = 0;
    this->bUseMagnifier = false;
    this->bLockMagnifierPosition = this->bLockMagnifierPositionHistory = false;
    this->SelectedSkydomeTypeIndex = 0;
    this->Exposure = 1.0f;
    this->IBLFactor = 2.0f;
    this->bDrawLightFrustum = false;
    this->bDrawBoundingBoxes = false;
    this->WireframeMode = WireframeMode::WIREFRAME_MODE_OFF;
    this->WireframeColor[0] = 0.0f;
    this->WireframeColor[1] = 1.0f;
    this->WireframeColor[2] = 0.0f;
    this->bShowControlsWindow = true;
    this->bShowProfilerWindow = true;
    this->bShowMilliseconds = false;
    this->bShowAverage = true;
    this->lastTimeStampsResetTime = 0;
    this->accumulatingFrameCount = 0;
}

//
// Magnifier UI Controls
//
void UIState::ToggleMagnifierLock()
{
    if (this->bUseMagnifier)
    {
        this->bLockMagnifierPositionHistory = this->bLockMagnifierPosition; // record histroy
        this->bLockMagnifierPosition = !this->bLockMagnifierPosition; // flip state
        const bool bLockSwitchedOn = !this->bLockMagnifierPositionHistory && this->bLockMagnifierPosition;
        const bool bLockSwitchedOff = this->bLockMagnifierPositionHistory && !this->bLockMagnifierPosition;
        if (bLockSwitchedOn)
        {
            const ImGuiIO& io = ImGui::GetIO();
            this->LockedMagnifiedScreenPositionX = static_cast<int>(io.MousePos.x);
            this->LockedMagnifiedScreenPositionY = static_cast<int>(io.MousePos.y);
            for (int ch = 0; ch < 3; ++ch) this->MagnifierParams.fBorderColorRGB[ch] = MAGNIFIER_BORDER_COLOR__LOCKED[ch];
        }
        else if (bLockSwitchedOff)
        {
            for (int ch = 0; ch < 3; ++ch) this->MagnifierParams.fBorderColorRGB[ch] = MAGNIFIER_BORDER_COLOR__FREE[ch];
        }
    }
}

// These are currently not bound to any mouse input and are here for convenience/reference.
// Mouse scroll is currently wired up to camera for panning and moving in the local Z direction.
// Any application that would prefer otherwise can utilize these for easily controlling the magnifier parameters through the desired input.
void UIState::AdjustMagnifierSize         (float increment /*= 0.05f*/){ MagnifierParams.fMagnifierScreenRadius = clamped(MagnifierParams.fMagnifierScreenRadius + increment, MAGNIFIER_RADIUS_MIN, MAGNIFIER_RADIUS_MAX); }
void UIState::AdjustMagnifierMagnification(float increment /*= 1.00f*/){ MagnifierParams.fMagnificationAmount   = clamped(MagnifierParams.fMagnificationAmount + increment, MAGNIFICATION_AMOUNT_MIN, MAGNIFICATION_AMOUNT_MAX); }

bool UIState::DevOption(bool* pBoolValue, const char* name)
{
    return ImGui::Checkbox(name, pBoolValue);
}

bool UIState::DevOption(int* pIntValue, const char* name, int minVal, int maxVal)
{
    return ImGui::SliderInt(name, pIntValue, minVal, maxVal);
}

bool UIState::DevOption(int* pEnumValue, const char* name, uint32_t count, const char* const* ppEnumNames)
{
    return ImGui::Combo(name, pEnumValue, ppEnumNames, count);
}

bool UIState::DevOption(int* pActiveIndex, uint32_t count, const char* const* ppEnumNames)
{
    bool pressed = false;

    ImGui::BeginGroup();
    for (uint32_t i = 0; i < count; i++)
    {
        pressed |= ImGui::RadioButton(ppEnumNames[i], pActiveIndex, int(i));
    }
    ImGui::EndGroup();

    return pressed;
}

bool UIState::DevOption(float* pFloatValue, const char* name, float fMin, float fMax)
{
    return ImGui::SliderFloat(name, pFloatValue, fMin, fMax);
}

void UIState::Text(const char* text)
{
    ImGui::Text(text);
}
