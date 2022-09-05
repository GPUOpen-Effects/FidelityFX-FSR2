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

#pragma once

#include "base/FrameworkWindows.h"
#include "Renderer.h"
#include "UI.h"

//
// This is the main class, it manages the state of the sample and does all the high level work without touching the GPU directly.
// This class uses the GPU via the the SampleRenderer class. We would have a SampleRenderer instance for each GPU.
//
// This class takes care of:
//
//    - loading a scene (just the CPU data)
//    - updating the camera
//    - keeping track of time
//    - handling the keyboard
//    - updating the animation
//    - building the UI (but do not renders it)
//    - uses the SampleRenderer to update all the state to the GPU and do the rendering
//

class FSR2Sample : public FrameworkWindows
{
public:
    FSR2Sample(LPCSTR name);
    void OnParseCommandLine(LPSTR lpCmdLine, uint32_t* pWidth, uint32_t* pHeight) override;
    void OnCreate() override;
    void OnDestroy() override;
    void OnRender() override;
    bool OnEvent(MSG msg) override;
    void OnResize(bool forceRecreateResources = false) override;
    void OnUpdateDisplay() override;

    void BuildUI();
    void LoadScene(int sceneIndex);

    void OnUpdate();

    void HandleInput(const ImGuiIO& io);
    void UpdateCamera(Camera& cam, const ImGuiIO& io);

private:
    bool                        m_bIsBenchmarking;

    GLTFCommon*                 m_pGltfLoader = NULL;
    bool                        m_loadingScene = false;

    Renderer*                   m_pRenderer = NULL;
    UIState                     m_UIState;
    float                       m_fontSize;
    uint32_t                    m_renderWidth = 0;
    uint32_t                    m_renderHeight = 0;

    float                       m_time; // Time accumulator in seconds, used for animation.

    // json config file
    json                        m_jsonConfigFile;
    std::vector<std::string>    m_sceneNames;
    int                         m_activeCamera;

    bool                        m_bPlay;
    bool                        m_bDisableHalf = false;
    bool                        m_bEnableHalf = false;
    bool                        m_bDisplayHalf = false;

    bool UseSlowFallback(bool deviceSupport)
    {
        assert(!m_bEnableHalf || !m_bDisableHalf);
        if (m_bEnableHalf)
            return false;
        else
            if (m_bDisableHalf)
                return true;
            else
                return !deviceSupport;
    }

private:
	void RefreshRenderResolution()
	{
		switch (m_nUpscaleMode)
		{
        case UPSCALE_QUALITY_MODE_NONE:
            m_fUpscaleRatio = 1.0f;
            break;
		case UPSCALE_QUALITY_MODE_ULTRA_QUALITY:
            m_fUpscaleRatio = 1.3f;
			break;
		case UPSCALE_QUALITY_MODE_QUALITY:
            m_fUpscaleRatio = 1.5f;
			break;
		case UPSCALE_QUALITY_MODE_BALANCE:
            m_fUpscaleRatio = 1.7f;
			break;
		case UPSCALE_QUALITY_MODE_PERFORMANCE:
            m_fUpscaleRatio = 2.0f;
			break;
        case UPSCALE_QUALITY_MODE_ULTRA_PERFORMANCE:
            m_fUpscaleRatio = 3.0f;
            break;

        default: break;
		}

        if (m_UIState.bDynamicRes)
        {
            m_UIState.renderWidth = uint32_t(m_UIState.displayWidth);
            m_UIState.renderHeight = uint32_t(m_UIState.displayHeight);
        }
        else
        {
            m_UIState.renderWidth = uint32_t(m_UIState.displayWidth / m_fUpscaleRatio);
            m_UIState.renderHeight = uint32_t(m_UIState.displayHeight / m_fUpscaleRatio);
        }
        m_bUpscaleChanged = false;
	}

    bool m_bUpscaleChanged = true;

    // A collection of mip bias levels for the various quality modes.
    float mipBias[UPSCALE_QUALITY_MODE_COUNT] = {
        0.f + log2(1.f / 1.0f) - 1.0f + FLT_EPSILON, // -1.0        - UPSCALE_QUALITY_MODE_NONE
        0.f + log2(1.f / 1.3f) - 1.0f + FLT_EPSILON, // -1.38f      - UPSCALE_QUALITY_MODE_ULTRA_QUALITY
        0.f + log2(1.f / 1.5f) - 1.0f + FLT_EPSILON, // -1.58f      - UPSCALE_QUALITY_MODE_QUALITY
        0.f + log2(1.f / 1.7f) - 1.0f + FLT_EPSILON, // -1.76f      - UPSCALE_QUALITY_MODE_BALANCED
        0.f + log2(1.f / 2.0f) - 1.0f + FLT_EPSILON, // -2.0f       - UPSCALE_QUALITY_MODE_PERFORMANCE
        0.f + log2(1.f / 3.0f) - 1.0f + FLT_EPSILON, // -2.58f      - UPSCALE_QUALITY_MODE_ULTRA_PERFORMANCE
        0.0f                                         // 0.0f        - UPSCALE_QUALITY_MODE_CUSTOM
    };
	
    UpscaleQualityMode m_nUpscaleMode = UPSCALE_QUALITY_MODE_PERFORMANCE;
	float m_fUpscaleRatio = 2.0f;
    bool m_bDynamicRes = false;
    float m_fDynamicResMinPercentage = 1.0f;
    struct
    {
        UpscaleQualityMode scaleMode;
        float   upscaleRatio;
        float   mipBias;
        bool    useSharpen;
        float   sharpness;
        bool    useTAA;
        bool    dynamicRes;
        float   dynamicResMinPercent;
    } m_SavedUiValues[UPSCALE_TYPE_COUNT] = {
                             { UPSCALE_QUALITY_MODE_PERFORMANCE, 2.0f, -2.f,   true,  1.f, true,  false, 1.0f},     // UPSCALE_TYPE_POINT
                             { UPSCALE_QUALITY_MODE_PERFORMANCE, 2.0f, -2.f,   true,  1.f, true,  false, 1.0f},     // UPSCALE_TYPE_BILINEAR
                             { UPSCALE_QUALITY_MODE_PERFORMANCE, 2.0f, -2.f,   true,  1.f, true,  false, 1.0f},     // UPSCALE_TYPE_BICUBIC
                             { UPSCALE_QUALITY_MODE_PERFORMANCE, 2.0f, -2.f,   true,  1.f, true,  false, 1.0f},     // UPSCALE_TYPE_FSR_1_0 (w/ TAA)
                             { UPSCALE_QUALITY_MODE_PERFORMANCE, 2.0f, -2.f,   false, 1.f, true,  false, 1.0f},     // UPSCALE_TYPE_FSR_2_0
                             { UPSCALE_QUALITY_MODE_NONE,        1.0f,  0.f,   false, 1.f, true,  false, 1.0f},     // UPSCALE_TYPE_NATIVE
    };
};
