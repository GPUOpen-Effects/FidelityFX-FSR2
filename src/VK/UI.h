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

#include "PostProc/MagnifierPS.h"
#include <string>

enum TAA_MODE
{
    RM_DISABLED,
    RM_FFX_TAA,
};

typedef enum UpscaleType {
    UPSCALE_TYPE_POINT = 0,
    UPSCALE_TYPE_BILINEAR = 1,
    UPSCALE_TYPE_BICUBIC = 2,
    UPSCALE_TYPE_FSR_1_0 = 3,
    UPSCALE_TYPE_FSR_2_0 = 4,
    UPSCALE_TYPE_NATIVE = 5,

    // add above this.
    UPSCALE_TYPE_COUNT
} UpscaleType;

typedef enum UpscaleQualityMode {
    UPSCALE_QUALITY_MODE_NONE = 0,
    UPSCALE_QUALITY_MODE_ULTRA_QUALITY = 1,
    UPSCALE_QUALITY_MODE_QUALITY = 2,
    UPSCALE_QUALITY_MODE_BALANCE = 3,
    UPSCALE_QUALITY_MODE_PERFORMANCE = 4,
    UPSCALE_QUALITY_MODE_ULTRA_PERFORMANCE = 5,
    UPSCALE_QUALITY_MODE_CUSTOM = 6,

    // add above this.
    UPSCALE_QUALITY_MODE_COUNT
} UpscaleQualityMode;

typedef enum ReactiveMaskMode {
    REACTIVE_MASK_MODE_OFF = 0,         // Nothing written to the reactive mask
    REACTIVE_MASK_MODE_ON = 1,          // Particles written to the reactive mask
    REACTIVE_MASK_MODE_AUTOGEN = 2,     // The mask is auto generated using FSR2's helper function

    // add above this.
    REACTIVE_MASK_MODE_COUNT
} ReactiveMaskMode;

struct UIState
{
    Camera  camera;
    bool    m_bCameraInertia = false;
    bool    m_bHeadBobbing = false;

    bool    m_bPlayAnimations = true;
    float   m_fTextureAnimationSpeed = 1.0f;
    int     m_activeScene = 0;
    bool    m_bAnimateSpotlight = false;

    //
    // WINDOW MANAGEMENT
    //
    bool bShowControlsWindow;
    bool bShowProfilerWindow;

    //
    // POST PROCESS CONTROLS
    //
    int   SelectedTonemapperIndex;
    float Exposure;
    float ExposureHdr = 1.f;

    bool bReset = false;

    int   nLightModulationMode = 0;
    bool  bRenderParticleSystem = true;
    bool  bRenderAnimatedTextures = true;
    bool  bUseMagnifier;
    bool  bLockMagnifierPosition;
    bool  bLockMagnifierPositionHistory;
    int   LockedMagnifiedScreenPositionX;
    int   LockedMagnifiedScreenPositionY;
    CAULDRON_VK::MagnifierPS::PassParameters MagnifierParams;

    const std::string* m_pScreenShotName = NULL;

    // FSR
    uint32_t displayWidth = 0;
    uint32_t displayHeight = 0;
    uint32_t renderWidth = 0;
    uint32_t renderHeight = 0;
    bool    bVsyncOn = true;

    // NOTE: These should always match the values in m_SavedUiValues for m_nUpscaleType.
    UpscaleType  m_nUpscaleType = UPSCALE_TYPE_FSR_2_0;
    float   mipBias = -2.0f;
    bool    bUseRcas = false;
    bool    bUseTAA = true;
    float   sharpening = 0.8f;

    // Dynamic resolution
    bool bDynamicRes = false;

    // TAA
    TAA_MODE                    taaMode = RM_FFX_TAA;
    unsigned int                closestVelocitySamplePattern = 0; // 5 samples
    float                       Feedback = 15.f / 16.f;

    // FSR2 reactive mask
    ReactiveMaskMode            nReactiveMaskMode = REACTIVE_MASK_MODE_ON;
    float                       fFsr2AutoReactiveScale = 1.f;
    float                       fFsr2AutoReactiveThreshold = 0.2f;
    float                       fFsr2AutoReactiveBinaryValue = 0.9f;
    bool                        bFsr2AutoReactiveTonemap = true;
    bool                        bFsr2AutoReactiveInverseTonemap = false;
    bool                        bFsr2AutoReactiveThreshold = true;
    bool                        bFsr2AutoReactiveUseMax = true;

    // FSR2 composition mask
    bool                        bCompositionMask = true;

    // FSR2
    bool                        bUseDebugOut = false;
    int                         nDebugBlitSurface = 6; // FFX_FSR2_RESOURCE_IDENTIFIER_INTERNAL_UPSCALED_COLOR
    int                         nDebugOutMappingR = 0;
    int                         nDebugOutMappingG = 1;
    int                         nDebugOutMappingB = 2;
    float                       v2DebugOutMappingR[2] = { 0.f, 1.f };
    float                       v2DebugOutMappingG[2] = { 0.f, 1.f };
    float                       v2DebugOutMappingB[2] = { 0.f, 1.f };

    float                       v4DebugSliderValues[8] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };

    //
    // APP/SCENE CONTROLS
    //
    float IBLFactor;
    float EmissiveFactor;
    double deltaTime;

    bool    bEnableFrameRateLimiter = false;
    bool    bUseGPUFrameRateLimiter = false;
    double  targetFrameTime;

    int   SelectedSkydomeTypeIndex;
    bool  bDrawBoundingBoxes;
    bool  bDrawLightFrustum;

    enum class WireframeMode : int
    {
        WIREFRAME_MODE_OFF = 0,
        WIREFRAME_MODE_SHADED = 1,
        WIREFRAME_MODE_SOLID_COLOR = 2,
    };

    WireframeMode WireframeMode;
    float         WireframeColor[3];

    //
    // PROFILER CONTROLS
    //
    bool  bShowMilliseconds;
    bool  bShowAverage;

    struct AccumulatedTimeStamp
    {
        float sum;
        float minimum;
        float maximum;

        AccumulatedTimeStamp()
            : sum(0.0f), minimum(FLT_MAX), maximum(0)
        {
        }
    };

    std::vector<AccumulatedTimeStamp> displayedTimeStamps;
    std::vector<AccumulatedTimeStamp> accumulatingTimeStamps;
    double lastTimeStampsResetTime;
    uint32_t accumulatingFrameCount;

    int32_t                     m_baloonID = -1;
    bool                        m_baloonInit = false;
    float                       m_BaloonAmplitude = 0.05f;
    math::Matrix4               m_InitBaloonTransform = {};
    math::Matrix4               m_CurBaloonTransform = {};
    bool                        m_bBaloonAttachToCamera = true;
    float                       baloon_offset_x = 0.05f;
    float                       baloon_offset_y = -0.08f;
    float                       baloon_offset_z = -0.17f;
    float                       baloon_offset_scale = 0.05f;
    Vectormath::Vector3 baloon_pos = { 0.0f, 0.0f, 0.0f };
    Vectormath::Vector3 baloon_tip_pos = { 0.0f, 0.0f, 0.0f };

    // -----------------------------------------------

    void Initialize();

    void ToggleMagnifierLock();
    void AdjustMagnifierSize(float increment = 0.05f);
    void AdjustMagnifierMagnification(float increment = 1.00f);

    bool DevOption(bool* pBoolValue, const char* name);
    bool DevOption(int* pIntValue, const char* name, int minVal, int maxVal);
    bool DevOption(int* pEnumValue, const char* name, uint32_t count, const char* const* ppEnumNames);
    bool DevOption(int* pActiveIndex, uint32_t count, const char* const* ppEnumNames);
    bool DevOption(float* pFloatValue, const char* name, float fMin = 0.0f, float fMax = 1.0f);
    void Text(const char* text);

    template<typename T>
    bool DevOption(T& flags, uint32_t bitIdx, const char* name)
    {
        static_assert(sizeof(T) <= sizeof(uint64_t));
        bool bSet = uint64_t(flags) & (1ULL << bitIdx);
        bool bResult = DevOption(&bSet, name);
        if (bSet)
        {
            flags = T(uint64_t(flags) | (1ULL << bitIdx));
        }
        else
        {
            flags = T(uint64_t(flags) & ~(1ULL << bitIdx));
        }
        return bResult;
    }
};
