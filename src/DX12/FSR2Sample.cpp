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
#include <intrin.h>

#include "FSR2Sample.h"

#define USE_INVERTED_INFINITE_PROJECTION 1

static constexpr float MagnifierBorderColor_Locked[3] = { 0.002f, 0.72f, 0.0f };
static constexpr float MagnifierBorderColor_Free[3]   = { 0.72f, 0.002f, 0.0f };

//static constexpr float AMD_PI = 3.1415926535897932384626433832795f;
//static constexpr float AMD_PI_OVER_2 = 1.5707963267948966192313216916398f;
//static constexpr float AMD_PI_OVER_4 = 0.78539816339744830961566084581988f;

FSR2Sample::FSR2Sample(LPCSTR name) : FrameworkWindows(name)
{
    m_time = 0;
    m_bPlay = true;

    m_pGltfLoader = NULL;
}

//--------------------------------------------------------------------------------------
//
// OnParseCommandLine
//
//--------------------------------------------------------------------------------------
void FSR2Sample::OnParseCommandLine(LPSTR lpCmdLine, uint32_t* pWidth, uint32_t* pHeight)
{
    // set some default values
    *pWidth = 1920;
    *pHeight = 1080;
    m_UIState.m_activeScene = 0; //load the first one by default
    m_VsyncEnabled = false;
    m_bIsBenchmarking = false;
    m_fontSize = 13.f; // default value overridden by a json file if available
    m_isCpuValidationLayerEnabled = false;
    m_isGpuValidationLayerEnabled = false;
    m_activeCamera = 1;
    m_stablePowerState = false;

    //read globals
    auto process = [&](json jData)
    {
        *pWidth = jData.value("width", *pWidth);
        *pHeight = jData.value("height", *pHeight);
        m_fullscreenMode = jData.value("presentationMode", m_fullscreenMode);
        m_UIState.m_activeScene = jData.value("activeScene", m_UIState.m_activeScene);
        m_activeCamera = jData.value("activeCamera", m_activeCamera);
        m_isCpuValidationLayerEnabled = jData.value("CpuValidationLayerEnabled", m_isCpuValidationLayerEnabled);
        m_isGpuValidationLayerEnabled = jData.value("GpuValidationLayerEnabled", m_isGpuValidationLayerEnabled);
        m_VsyncEnabled = jData.value("vsync", m_VsyncEnabled);
        m_FreesyncHDROptionEnabled = jData.value("FreesyncHDROptionEnabled", m_FreesyncHDROptionEnabled);
        m_bIsBenchmarking = jData.value("benchmark", m_bIsBenchmarking);
        m_stablePowerState = jData.value("stablePowerState", m_stablePowerState);
        m_fontSize = jData.value("fontsize", m_fontSize);
    };

    // read config file
    //
    {
        std::ifstream f("FSR2_Sample.json");
        if (!f)
        {
            MessageBox(NULL, "Config file not found!\n", "Cauldron Panic!", MB_ICONERROR);
            exit(0);
        }

        try
        {
            f >> m_jsonConfigFile;
        }
        catch (json::parse_error)
        {
            MessageBox(NULL, "Error parsing GLTFSample.json!\n", "Cauldron Panic!", MB_ICONERROR);
            exit(0);
        }
    }


    json globals = m_jsonConfigFile["globals"];
    process(globals);

    // read json globals from commandline (and override values from config file)
    //
    try
    {
        if (strlen(lpCmdLine) > 0)
        {
            auto j3 = json::parse(lpCmdLine);
            process(j3);
        }
    }
    catch (json::parse_error)
    {
        Trace("Error parsing commandline\n");
        exit(0);
    }


    // get the list of scenes
    for (const auto& scene : m_jsonConfigFile["scenes"])
        m_sceneNames.push_back(scene["name"]);


}

//--------------------------------------------------------------------------------------
//
// OnCreate
//
//--------------------------------------------------------------------------------------
void FSR2Sample::OnCreate()
{
    //init the shader compiler
    InitDirectXCompiler();
    CreateShaderCache();

    // Create a instance of the renderer and initialize it, we need to do that for each GPU
    m_pRenderer = new Renderer();
#if USE_INVERTED_INFINITE_PROJECTION
    m_pRenderer->OnCreate(&m_device, &m_swapChain, m_fontSize, true);
#else
    m_pRenderer->OnCreate(&m_device, &m_swapChain, m_fontSize, false);
#endif

    // init GUI (non gfx stuff)
    ImGUI_Init((void*)m_windowHwnd);
    m_UIState.Initialize();

    OnResize();
    OnUpdateDisplay();

    // Init Camera, looking at the origin
    m_UIState.camera.LookAt(math::Vector4(0, 0, 5, 0), math::Vector4(0, 0, 0, 0));
}

//--------------------------------------------------------------------------------------
//
// OnDestroy
//
//--------------------------------------------------------------------------------------
void FSR2Sample::OnDestroy()
{
    ImGUI_Shutdown();

    m_device.GPUFlush();

    m_pRenderer->UnloadScene();
    m_pRenderer->OnDestroyWindowSizeDependentResources();
    m_pRenderer->OnDestroy();

    delete m_pRenderer;

    //shut down the shader compiler 
    DestroyShaderCache(&m_device);

    if (m_pGltfLoader)
    {
        delete m_pGltfLoader;
        m_pGltfLoader = NULL;
    }
}

//--------------------------------------------------------------------------------------
//
// OnEvent, win32 sends us events and we forward them to ImGUI
//
//--------------------------------------------------------------------------------------
bool FSR2Sample::OnEvent(MSG msg)
{
    if (ImGUI_WndProcHandler(msg.hwnd, msg.message, msg.wParam, msg.lParam))
        return true;

    // handle function keys (F1, F2...) here, rest of the input is handled
    // by imGUI later in HandleInput() function
    const WPARAM& KeyPressed = msg.wParam;
    switch (msg.message)
    {
    case WM_KEYUP:
    case WM_SYSKEYUP:
        /* WINDOW TOGGLES */
        if (KeyPressed == VK_F11) m_UIState.bShowControlsWindow ^= 1;
        if (KeyPressed == VK_F12) m_UIState.bShowProfilerWindow ^= 1;

        if (KeyPressed == VK_F1) {
            // Native
            m_SavedUiValues[m_UIState.m_nUpscaleType].scaleMode = m_nUpscaleMode = UPSCALE_QUALITY_MODE_NONE;
            m_SavedUiValues[m_UIState.m_nUpscaleType].upscaleRatio = m_fUpscaleRatio = 1.f;
            m_SavedUiValues[m_UIState.m_nUpscaleType].mipBias = m_UIState.mipBias = mipBias[UPSCALE_QUALITY_MODE_NONE];
            m_bUpscaleChanged = true;
        }
        if (KeyPressed == VK_F2) {
            // Ultra Quality
            m_SavedUiValues[m_UIState.m_nUpscaleType].scaleMode = m_nUpscaleMode = UPSCALE_QUALITY_MODE_ULTRA_QUALITY;
            m_SavedUiValues[m_UIState.m_nUpscaleType].mipBias = m_UIState.mipBias = mipBias[UPSCALE_QUALITY_MODE_ULTRA_QUALITY];
            m_bUpscaleChanged = true;
        }
        if (KeyPressed == VK_F3) {
            // Quality
            m_SavedUiValues[m_UIState.m_nUpscaleType].scaleMode = m_nUpscaleMode = UPSCALE_QUALITY_MODE_QUALITY;
            m_SavedUiValues[m_UIState.m_nUpscaleType].mipBias = m_UIState.mipBias = mipBias[UPSCALE_QUALITY_MODE_QUALITY];
            m_bUpscaleChanged = true;
        }
        if (KeyPressed == VK_F4) {
            // Balanced
            m_SavedUiValues[m_UIState.m_nUpscaleType].scaleMode = m_nUpscaleMode = UPSCALE_QUALITY_MODE_BALANCE;
            m_SavedUiValues[m_UIState.m_nUpscaleType].mipBias = m_UIState.mipBias = mipBias[UPSCALE_QUALITY_MODE_BALANCE];
            m_bUpscaleChanged = true;
        }
        if (KeyPressed == VK_F5) {
            // Performance
            m_SavedUiValues[m_UIState.m_nUpscaleType].scaleMode = m_nUpscaleMode = UPSCALE_QUALITY_MODE_PERFORMANCE;
            m_SavedUiValues[m_UIState.m_nUpscaleType].mipBias = m_UIState.mipBias = mipBias[UPSCALE_QUALITY_MODE_PERFORMANCE];
            m_bUpscaleChanged = true;
        }
        if (KeyPressed == VK_F6) {
            // Ultra Performance
            m_SavedUiValues[m_UIState.m_nUpscaleType].scaleMode = m_nUpscaleMode = UPSCALE_QUALITY_MODE_ULTRA_PERFORMANCE;
            m_SavedUiValues[m_UIState.m_nUpscaleType].mipBias = m_UIState.mipBias = mipBias[UPSCALE_QUALITY_MODE_ULTRA_PERFORMANCE];
            m_bUpscaleChanged = true;
        }
        break;
    }

    return true;
}

//--------------------------------------------------------------------------------------
//
// OnResize
//
//--------------------------------------------------------------------------------------
void FSR2Sample::OnResize(bool forceResizeRender)
{
    bool resizeDisplay = forceResizeRender ||
        (m_UIState.displayWidth != m_Width || m_UIState.displayHeight != m_Height);

    m_UIState.displayWidth = m_Width;
    m_UIState.displayHeight = m_Height;

    RefreshRenderResolution();

    bool resizeRender = forceResizeRender ||
        (m_UIState.renderWidth != m_renderWidth || m_UIState.renderHeight != m_renderHeight);
    m_renderWidth = m_UIState.renderWidth;
    m_renderHeight = m_UIState.renderHeight;

    if (resizeDisplay || resizeRender)
    {
        // Flush GPU
        m_device.GPUFlush();
    }


    if (resizeDisplay || resizeRender)
    {
        if (m_pRenderer != NULL)
        {
            m_pRenderer->OnDestroyWindowSizeDependentResources();
        }

        // if resizing but not minimizing the recreate it with the new size
        //
        if (m_UIState.displayWidth > 0 && m_UIState.displayHeight > 0)
        {
            if (m_pRenderer != NULL)
            {
                m_UIState.bReset = true;
                m_pRenderer->OnCreateWindowSizeDependentResources(&m_swapChain, &m_UIState);
            }
        }
    }

    float aspect = m_UIState.renderWidth * 1.f / m_UIState.renderHeight;

#if USE_INVERTED_INFINITE_PROJECTION
        m_UIState.camera.SetFov(AMD_PI_OVER_2/aspect, aspect, 0.1f);
#else
        m_UIState.camera.SetFov(AMD_PI_OVER_2 / aspect, aspect, 0.1f, 1000.0f);
#endif
}

//--------------------------------------------------------------------------------------
//
// UpdateDisplay
//
//--------------------------------------------------------------------------------------
void FSR2Sample::OnUpdateDisplay()
{
    // Destroy resources (if we are not minimized)
    if (m_pRenderer)
    {
        m_pRenderer->OnUpdateDisplayDependentResources(&m_swapChain);
    }
}

//--------------------------------------------------------------------------------------
//
// LoadScene
//
//--------------------------------------------------------------------------------------
void FSR2Sample::LoadScene(int sceneIndex)
{
    json scene = m_jsonConfigFile["scenes"][sceneIndex];

    // release everything and load the GLTF, just the light json data, the rest (textures and geometry) will be done in the main loop
    if (m_pGltfLoader != NULL)
    {
        m_pRenderer->UnloadScene();
        m_pRenderer->OnDestroyWindowSizeDependentResources();
        m_pRenderer->OnDestroy();
        m_pGltfLoader->Unload();

        // Make sure the actual resolution is updated no matter previous render size
        RefreshRenderResolution();
#if USE_INVERTED_INFINITE_PROJECTION
        m_pRenderer->OnCreate(&m_device, &m_swapChain, m_fontSize, true);
#else
        m_pRenderer->OnCreate(&m_device, &m_swapChain, m_fontSize, false);
#endif
        m_pRenderer->OnCreateWindowSizeDependentResources(&m_swapChain, &m_UIState);
    }

    delete(m_pGltfLoader);
    m_pGltfLoader = new GLTFCommon();
    if (m_pGltfLoader->Load(scene["directory"], scene["filename"]) == false)
    {
        MessageBox(NULL, "The selected model couldn't be found, please check the documentation", "Cauldron Panic!", MB_ICONERROR);
        exit(0);
    }

    // Load the UI settings, and also some defaults cameras and lights, in case the GLTF has none
    {
#define LOAD(j, key, val) val = j.value(key, val)

        // global settings
        LOAD(scene, "toneMapper", m_UIState.SelectedTonemapperIndex);
        LOAD(scene, "skyDomeType", m_UIState.SelectedSkydomeTypeIndex);
        LOAD(scene, "exposure", m_UIState.Exposure);
        LOAD(scene, "iblFactor", m_UIState.IBLFactor);
        LOAD(scene, "skyDomeType", m_UIState.SelectedSkydomeTypeIndex);

        // Add a default light in case there are none
        if (m_pGltfLoader->m_lights.size() == 0)
        {
            if(scene["name"]==std::string("Sponza"))
            {
                {
                    tfNode n;
                    n.m_transform.LookAt(math::Vector4(20, 100, -10, 0), math::Vector4(0, 0, 0, 0));

                    tfLight l;
                    l.m_type = tfLight::LIGHT_DIRECTIONAL;
                    l.m_intensity = scene.value("intensity", 1.0f);
                    l.m_color = math::Vector4(1.0f, 1.0f, 0.8f, 0.0f);
                    l.m_shadowResolution = 1024;

                    m_pGltfLoader->AddLight(n, l);
                }
                {
                    tfNode n;
                    n.m_transform.LookAt(math::Vector4(-2, 10, 1, 0), math::Vector4(0, 0, 0, 0));
                    
                    tfLight l;
                    l.m_type = tfLight::LIGHT_SPOTLIGHT;
                    l.m_intensity = scene.value("intensity", 1.0f);
                    l.m_color = math::Vector4(1.0f, 1.0f, 1.0f, 0.0f);
                    l.m_range = 15;
                    l.m_outerConeAngle = AMD_PI_OVER_4;
                    l.m_innerConeAngle = AMD_PI_OVER_4 * 0.9f;
                    l.m_shadowResolution = 1024;

                    m_pGltfLoader->AddLight(n, l);
                }
                {
                    tfNode n;
                    n.m_transform.m_translation = math::Vector4(-13.f, 1.9f, 0.f, 0.f);

                    tfLight l;
                    l.m_type = tfLight::LIGHT_POINTLIGHT;
                    l.m_intensity = scene.value("intensity", 1.0f) * 0.25f;
                    l.m_color = math::Vector4(1.0f, 0.95f, 0.8f, 0.0f);
                    l.m_range = 4;

                    m_pGltfLoader->AddLight(n, l);
                }
                {
                    tfNode n;
                    n.m_transform.m_translation = math::Vector4(13.f, 1.9f, 0.f, 0.f);

                    tfLight l;
                    l.m_type = tfLight::LIGHT_POINTLIGHT;
                    l.m_intensity = scene.value("intensity", 1.0f) * 0.25f;
                    l.m_color = math::Vector4(1.0f, 0.95f, 0.8f, 0.0f);
                    l.m_range = 10;

                    m_pGltfLoader->AddLight(n, l);
                }
            }
            else
            {
                tfNode n;
                n.m_transform.LookAt(PolarToVector(AMD_PI_OVER_2, 0.58f) * 3.5f, math::Vector4(0, 0, 0, 0));

                tfLight l;
                l.m_type = tfLight::LIGHT_SPOTLIGHT;
                l.m_intensity = scene.value("intensity", 1.0f);
                l.m_color = math::Vector4(1.0f, 1.0f, 1.0f, 0.0f);
                l.m_range = 15;
                l.m_outerConeAngle = AMD_PI_OVER_4;
                l.m_innerConeAngle = AMD_PI_OVER_4 * 0.9f;
                l.m_shadowResolution = 1024;

                m_pGltfLoader->AddLight(n, l);
            }
        }

        // Allocate shadow information (if any)
        m_pRenderer->AllocateShadowMaps(m_pGltfLoader);

        // set default camera
        json camera = scene["camera"];
        m_activeCamera = scene.value("activeCamera", m_activeCamera);
        math::Vector4 from = GetVector(GetElementJsonArray(camera, "defaultFrom", { 0.0, 0.0, 10.0 }));
        math::Vector4 to = GetVector(GetElementJsonArray(camera, "defaultTo", { 0.0, 0.0, 0.0 }));
        m_UIState.camera.LookAt(from, to);

        // set benchmarking state if enabled 
        if (m_bIsBenchmarking)
        {
            std::string deviceName;
            std::string driverVersion;
            m_device.GetDeviceInfo(&deviceName, &driverVersion);
            BenchmarkConfig(scene["BenchmarkSettings"], m_activeCamera, m_pGltfLoader, deviceName, driverVersion);
        }

        // indicate the mainloop we started loading a GLTF and it needs to load the rest (textures and geometry)
        m_loadingScene = true;
    }
}


//--------------------------------------------------------------------------------------
//
// OnUpdate
//
//--------------------------------------------------------------------------------------
void FSR2Sample::OnUpdate()
{
    ImGuiIO& io = ImGui::GetIO();

    //If the mouse was not used by the GUI then it's for the camera
    if (io.WantCaptureMouse)
    {
        io.MouseDelta.x = 0;
        io.MouseDelta.y = 0;
        io.MouseWheel = 0;
    }

    // Update Camera
    UpdateCamera(m_UIState.camera, io);

    // Keyboard & Mouse
    HandleInput(io);

    // Animation Update
    if (m_bPlay)
        m_time += (float)m_deltaTime / 1000.0f; // animation time in seconds

    if (m_pGltfLoader)
    {
        for( int i = 0; i<32; ++i)
            m_pGltfLoader->SetAnimationTime(i, m_time);

        if (m_UIState.m_balloonID >= 0) {
            std::vector<tfNode>& pNodes = m_pGltfLoader->m_nodes;
            math::Matrix4* pNodesMatrices = m_pGltfLoader->m_animatedMats.data();
            if (!m_UIState.m_balloonInit) {
                m_pGltfLoader->TransformScene(0, math::Matrix4::identity());
                m_UIState.m_InitBaloonTransform= pNodesMatrices[m_UIState.m_balloonID];
                m_UIState.m_CurBaloonTransform = pNodesMatrices[m_UIState.m_balloonID];
                m_UIState.m_balloonInit = true;
            }
            float y = std::sin(m_time) * m_UIState.m_BalloonAmplitude;
            if (m_UIState.m_bBalloonAttachToCamera) {
                Vectormath::Vector3  eye = m_UIState.camera.GetPosition().getXYZ() + m_UIState.camera.GetDirection().getXYZ() * m_UIState.balloon_offset_z;
                Vectormath::Vector3  look = m_UIState.camera.GetPosition().getXYZ() + m_UIState.camera.GetDirection().getXYZ() * -2.0f;
                m_UIState.baloon_pos = m_UIState.baloon_pos + (eye - m_UIState.baloon_pos) * (1.0f - std::expf(50.0f * -(float)m_deltaTime / 1000.0f));
                m_UIState.baloon_tip_pos = m_UIState.baloon_tip_pos + (look - m_UIState.baloon_tip_pos) * (1.0f - std::expf(50.0f * -(float)m_deltaTime / 1000.0f));
                m_UIState.m_CurBaloonTransform = Vectormath::inverse(Vectormath::Matrix4::lookAt(Vectormath::Point3(m_UIState.baloon_pos), Vectormath::Point3(m_UIState.baloon_tip_pos), Vectormath::Vector3(0.0f, 1.0f, 0.0f))) *
                    Vectormath::Matrix4::translation(Vectormath::Vector3(m_UIState.balloon_offset_x, m_UIState.balloon_offset_y, 0.0f)) * //
                    Vectormath::Matrix4::rotation(-3.141592f / 2.0f, Vectormath::Vector3(1.0f, 0.0f, 0.0f)) *       //
                    Vectormath::Matrix4::rotation(-3.141592f / 2.0f, Vectormath::Vector3(0.0f, 0.0f, 1.0f)) *       //
                    Vectormath::Matrix4::scale(Vectormath::Vector3(m_UIState.balloon_offset_scale, m_UIState.balloon_offset_scale, m_UIState.balloon_offset_scale));
            }
            pNodesMatrices[m_UIState.m_balloonID] = Vectormath::Matrix4::translation(Vectormath::Vector3(0.0f, y, 0.0f)) * m_UIState.m_CurBaloonTransform;
        }


        m_pGltfLoader->TransformScene(0, math::Matrix4::identity());
    }

    m_UIState.deltaTime = m_deltaTime;

    // frame rate limiter
    if (m_UIState.bEnableFrameRateLimiter && !m_UIState.bUseGPUFrameRateLimiter)
    {
        static double lastFrameTime = MillisecondsNow();
        double timeNow = MillisecondsNow();
        double deltaTime = timeNow - lastFrameTime;
        if (deltaTime < m_UIState.targetFrameTime)
        {
            Sleep(DWORD(m_UIState.targetFrameTime - deltaTime));
            timeNow += m_UIState.targetFrameTime - deltaTime;
        }
        lastFrameTime = timeNow;
    }
}

void FSR2Sample::HandleInput(const ImGuiIO& io)
{
    auto fnIsKeyTriggered = [&io](char key) { return io.KeysDown[key] && io.KeysDownDuration[key] == 0.0f; };

    // Handle Keyboard/Mouse input here

    /* MAGNIFIER CONTROLS */
    if (fnIsKeyTriggered('L'))                       m_UIState.ToggleMagnifierLock();
    if (fnIsKeyTriggered('M') || io.MouseClicked[2]) m_UIState.bUseMagnifier ^= 1; // middle mouse / M key toggles magnifier

    // save current state to saved
    m_SavedUiValues[m_UIState.m_nUpscaleType].scaleMode = m_nUpscaleMode;
    m_SavedUiValues[m_UIState.m_nUpscaleType].upscaleRatio = m_fUpscaleRatio;
    m_SavedUiValues[m_UIState.m_nUpscaleType].mipBias = m_UIState.mipBias;
    m_SavedUiValues[m_UIState.m_nUpscaleType].useSharpen = m_UIState.bUseRcas;
    m_SavedUiValues[m_UIState.m_nUpscaleType].sharpness = m_UIState.sharpening;
    m_SavedUiValues[m_UIState.m_nUpscaleType].useTAA = m_UIState.bUseTAA;

    if (io.KeyCtrl == true)
    {
        if (fnIsKeyTriggered('1'))
        {
            m_bUpscaleChanged = true;
            m_UIState.m_nUpscaleType = UPSCALE_TYPE_POINT;
            m_UIState.bDynamicRes = false;
        }
        else if (fnIsKeyTriggered('2'))
        {
            m_bUpscaleChanged = true;
            m_UIState.m_nUpscaleType = UPSCALE_TYPE_BILINEAR;
            m_UIState.bDynamicRes = false;
        }
        else if (fnIsKeyTriggered('3'))
        {
            m_bUpscaleChanged = true;
            m_UIState.m_nUpscaleType = UPSCALE_TYPE_BICUBIC;
            m_UIState.bDynamicRes = false;
        }
        else if (fnIsKeyTriggered('4'))
        {
            m_bUpscaleChanged = true;
            m_UIState.m_nUpscaleType = UPSCALE_TYPE_FSR_1_0;
            m_UIState.bDynamicRes = false;
        }
        else if (fnIsKeyTriggered('5'))
        {
            m_bUpscaleChanged = true;
            m_UIState.m_nUpscaleType = UPSCALE_TYPE_FSR_2_0;
        }
        else if (fnIsKeyTriggered('0'))
        {
            m_bUpscaleChanged = true;
            m_UIState.m_nUpscaleType = UPSCALE_TYPE_NATIVE;
            m_UIState.bDynamicRes = false;

            // force reset values for native
            m_SavedUiValues[m_UIState.m_nUpscaleType].scaleMode = UPSCALE_QUALITY_MODE_NONE;
            m_SavedUiValues[m_UIState.m_nUpscaleType].upscaleRatio = 1.f;
            m_SavedUiValues[m_UIState.m_nUpscaleType].mipBias = 0.f;
        }
    }

    m_nUpscaleMode = m_SavedUiValues[m_UIState.m_nUpscaleType].scaleMode;
    m_fUpscaleRatio = m_SavedUiValues[m_UIState.m_nUpscaleType].upscaleRatio;
    m_UIState.mipBias = m_SavedUiValues[m_UIState.m_nUpscaleType].mipBias;
    m_UIState.bUseRcas = m_SavedUiValues[m_UIState.m_nUpscaleType].useSharpen;
    m_UIState.sharpening = m_SavedUiValues[m_UIState.m_nUpscaleType].sharpness;
    m_UIState.bUseTAA = m_SavedUiValues[m_UIState.m_nUpscaleType].useTAA;

    if (m_bUpscaleChanged)
    {
        OnResize(true);
    }

    if (io.MouseClicked[1] && m_UIState.bUseMagnifier) // right mouse click
        m_UIState.ToggleMagnifierLock();
}

void FSR2Sample::UpdateCamera(Camera& cam, const ImGuiIO& io)
{
    float yaw = cam.GetYaw();
    float pitch = cam.GetPitch();
    float distance = cam.GetDistance();

    cam.UpdatePreviousMatrices(); // set previous view matrix

    // Sets Camera based on UI selection (WASD, Orbit or any of the GLTF cameras)
    if ((io.KeyCtrl == false) && (io.MouseDown[0] == true))
    {
        yaw -= io.MouseDelta.x / 100.f;
        pitch += io.MouseDelta.y / 100.f;
    }

    // Choose camera movement depending on setting
    if (m_activeCamera == 0)
    {
        // If nothing has changed, don't calculate an update (we are getting micro changes in view causing bugs)
        if (!io.MouseWheel && (!io.MouseDown[0] || (!io.MouseDelta.x && !io.MouseDelta.y)))
            return;

        //  Orbiting
        distance -= (float)io.MouseWheel / 3.0f;
        distance = std::max<float>(distance, 0.1f);

        bool panning = (io.KeyCtrl == true) && (io.MouseDown[0] == true);

        cam.UpdateCameraPolar(yaw, pitch,
            panning ? -io.MouseDelta.x / 100.0f : 0.0f,
            panning ? io.MouseDelta.y / 100.0f : 0.0f,
            distance);
    }
    else if (m_activeCamera == 1)
    {
        //  WASD
        cam.SetSpeed(io.KeyCtrl ? 1.0f : 0.01f);
        cam.UpdateCameraWASD(yaw, pitch, io.KeysDown, io.DeltaTime);

        if (m_UIState.m_bHeadBobbing)
        {
            static math::Vector4 bob = math::Vector4(0.f);

            math::Vector4 eyePos = cam.GetPosition();

            // remove old headbob
            eyePos -= bob;

            // compute new headbob
            math::Matrix4 view = cam.GetView();
            math::Vector4 side = view.getRow(0) * 0.02f;
            bob = side * sin(5 * m_time);
            bob.setY(bob.getY() + 0.04f * sin(5* 2*m_time));

            // apply new headbob
            eyePos += bob;

            view.setCol3(eyePos);
            math::Vector4 viewDir = -cam.GetDirection();
            view.setCol2(viewDir);
            cam.SetMatrix(view);
        }
    }
    else if (m_activeCamera > 1)
    {
        // Use a camera from the GLTF
        m_pGltfLoader->GetCamera(m_activeCamera - 2, &cam);
    }
}

//--------------------------------------------------------------------------------------
//
// OnRender, updates the state from the UI, animates, transforms and renders the scene
//
//--------------------------------------------------------------------------------------
void FSR2Sample::OnRender()
{
    // Do any start of frame necessities
    BeginFrame();

    ImGUI_UpdateIO(m_UIState.displayWidth, m_UIState.displayHeight);

    if (m_UIState.bDynamicRes)
    {
        // Quick dynamic resolution test: generate a resolution that fits in between the defined upscale factor
        // and native resolution.
        float sinTime = ((float)(sin(MillisecondsNow() / 1000.0)) + 1.0f) / 2.0f;
        float thisFrameUpscaleRatio = (1.0f / m_fUpscaleRatio) + (1.0f - 1.0f / m_fUpscaleRatio) * sinTime;

        m_UIState.renderWidth = (uint32_t)((float)m_Width * thisFrameUpscaleRatio);
        m_UIState.renderHeight = (uint32_t)((float)m_Height * thisFrameUpscaleRatio);
        m_UIState.mipBias = log2f(float(m_UIState.renderWidth) / m_UIState.displayWidth) - 1.0f;
    }
    else
    {
        float thisFrameUpscaleRatio = 1.f/m_SavedUiValues[m_UIState.m_nUpscaleType].upscaleRatio;
        m_UIState.renderWidth = (uint32_t)((float)m_Width * thisFrameUpscaleRatio);
        m_UIState.renderHeight = (uint32_t)((float)m_Height * thisFrameUpscaleRatio);
        m_UIState.mipBias = log2f(float(m_UIState.renderWidth) / m_UIState.displayWidth) - 1.0f;
    }

    ImGui::NewFrame();

    if (m_loadingScene)
    {
        // the scene loads in chunks, that way we can show a progress bar
        static int loadingStage = 0;
        loadingStage = m_pRenderer->LoadScene(m_pGltfLoader, loadingStage);
        if (loadingStage == 0)
        {
            m_time = 0;
            m_loadingScene = false;

            const json& j3 = m_pGltfLoader->j3;
            if (j3.find("meshes") != j3.end()) {
                const json& nodes = j3["nodes"];
                for (uint32_t i = 0; i < nodes.size(); i++) {
                    const json& node = nodes[i];
                    std::string name = GetElementString(node, "name", "unnamed");

                    if (name == "baloon") {
                        m_UIState.m_balloonID = i;
                    }
                }
            }
        }
    }
    else if (m_pGltfLoader && m_bIsBenchmarking)
    {
        // Benchmarking takes control of the time, and exits the app when the animation is done
        std::vector<TimeStamp> timeStamps = m_pRenderer->GetTimingValues();
        m_time = BenchmarkLoop(timeStamps, &m_UIState.camera, m_pRenderer->GetScreenshotFileName());
    }
    else
    {
        BuildUI();  // UI logic. Note that the rendering of the UI happens later.
        OnUpdate(); // Update camera, handle keyboard/mouse input
    }

    // Do Render frame using AFR
    m_pRenderer->OnRender(&m_UIState, m_UIState.camera, &m_swapChain);

    // Framework will handle Present and some other end of frame logic
    EndFrame();
}


//--------------------------------------------------------------------------------------
//
// WinMain
//
//--------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow)
{
    LPCSTR Name = "FidelityFX Super Resolution 2.2";

    // create new DX sample
    return RunFramework(hInstance, lpCmdLine, nCmdShow, new FSR2Sample(Name));
}

#if !defined(_DEBUG)
CAULDRON_APP_USE_DX12_AGILITY_SDK(600, u8".\\D3D12\\")
#endif
